#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include "EncounterBatchTactics.h"
#include "../../Source/Tools/interface.h"
#include "../../Source/Tools/ZHANSHU.h"

#define ENABLE_ACMI_REPLAY 1
#define ACMI_REPLAY_COUNT 10

static const char* kReplayDirectory = "agent_mvp/data_real/replays";

using json = nlohmann::json;

struct Unit7D {
    double type_id = 0.0;
    double speed = 0.0;
    double sensor = 0.0;
    double missile = 0.0;
    double lon = 0.0;
    double lat = 0.0;
    double alive = 1.0;
};

struct Unit2D {
    double final_missile = 0.0;
    double final_alive = 0.0;
};

struct BattleOutcome {
    std::vector<Unit2D> red_final;
    std::vector<Unit2D> blue_final;
    int red_win = 0;
    int elapsed_steps = 0;
    std::string termination_reason;
    bool red_objective_complete = false;
    bool blue_objective_complete = false;
    std::vector<json> rollout_rows;
};

struct TerminationStatus {
    bool done = false;
    std::string reason = "none";
};

struct RolloutEventAccumulator {
    int red_fire_count_delta = 0;
    int blue_fire_count_delta = 0;
    int red_kill_delta = 0;
    int blue_kill_delta = 0;
    int red_dodge_trigger_count = 0;
    int blue_dodge_trigger_count = 0;
    bool red_first_contact_flag = false;
    bool blue_first_contact_flag = false;
    bool red_contact_flag = false;
    bool blue_contact_flag = false;
    bool red_first_fire_flag = false;
    bool blue_first_fire_flag = false;
    bool red_warning_flag = false;
    bool blue_warning_flag = false;
    bool red_retarget_flag = false;
    bool blue_retarget_flag = false;
    bool red_first_kill_flag = false;
    bool blue_first_kill_flag = false;
    bool red_objective_complete_flag = false;
    bool blue_objective_complete_flag = false;
    bool termination_flag = false;
};

struct EpisodeEventState {
    bool red_contact_seen = false;
    bool blue_contact_seen = false;
    bool red_fire_seen = false;
    bool blue_fire_seen = false;
    bool red_kill_seen = false;
    bool blue_kill_seen = false;
    double red_reward_cumulative = 0.0;
    double blue_reward_cumulative = 0.0;
};

struct GeoPoint {
    double lon = 0.0;
    double lat = 0.0;
};

struct BattlefieldSquare {
    GeoPoint center;
    double top_left_lon = 0.0;
    double top_left_lat = 0.0;
    double side_length_m = 0.0;
    double safe_min_lon = 0.0;
    double safe_max_lon = 0.0;
    double safe_min_lat = 0.0;
    double safe_max_lat = 0.0;
};

static constexpr double kRedBaseLongitude = 116.200000;
static constexpr double kRedBaseLatitude = 21.577106;
static constexpr double kBlueBaseLongitude = 119.800000;
static constexpr double kBlueBaseLatitude = 21.577106;
static constexpr double kBattlefieldSquareSideMeters = 600000.0;
static constexpr double kBattlefieldEdgeMarginMeters = 60000.0;
static constexpr double kReturnCompletionRadiusMeters = 15000.0;
static constexpr int kMissionCompletionHoldSteps = 20;
static constexpr int kMissionLogIntervalSteps = 200;
static constexpr int kMissionSafetyMaxSteps = 9000;
static constexpr double kSimulationStepSeconds = 0.1;
static constexpr int kRolloutExportStrideSteps = 10;

static double Clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

static double DegToRad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

static double RadToDeg(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
}

static double NormalizeHeadingDegrees(double heading_deg) {
    if (!std::isfinite(heading_deg)) {
        return 0.0;
    }

    double normalized = std::fmod(heading_deg, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

static double LongitudeDegreesForMeters(double meters, double latitude_deg) {
    const double cos_lat = (std::max)(0.2, std::cos(DegToRad(latitude_deg)));
    return meters / (111320.0 * cos_lat);
}

static double LatitudeDegreesForMeters(double meters) {
    return meters / 111000.0;
}

static double GeoDistanceMeters(double lon1, double lat1, double lon2, double lat2) {
    const double earth_radius_m = 6371000.0;
    const double dlat = DegToRad(lat2 - lat1);
    const double dlon = DegToRad(lon2 - lon1);
    const double a = std::sin(dlat * 0.5) * std::sin(dlat * 0.5) +
                     std::cos(DegToRad(lat1)) * std::cos(DegToRad(lat2)) *
                         std::sin(dlon * 0.5) * std::sin(dlon * 0.5);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return earth_radius_m * c;
}

static BattlefieldSquare BuildBattlefieldSquare() {
    BattlefieldSquare square;
    square.center = GeoPoint{
        (kRedBaseLongitude + kBlueBaseLongitude) * 0.5,
        (kRedBaseLatitude + kBlueBaseLatitude) * 0.5};
    square.side_length_m = kBattlefieldSquareSideMeters;

    const double half_lon_deg = LongitudeDegreesForMeters(square.side_length_m * 0.5, square.center.lat);
    const double half_lat_deg = LatitudeDegreesForMeters(square.side_length_m * 0.5);
    const double edge_lon_deg = LongitudeDegreesForMeters(kBattlefieldEdgeMarginMeters, square.center.lat);
    const double edge_lat_deg = LatitudeDegreesForMeters(kBattlefieldEdgeMarginMeters);

    square.top_left_lon = square.center.lon - half_lon_deg;
    square.top_left_lat = square.center.lat + half_lat_deg;
    square.safe_min_lon = square.center.lon - half_lon_deg + edge_lon_deg;
    square.safe_max_lon = square.center.lon + half_lon_deg - edge_lon_deg;
    square.safe_min_lat = square.center.lat - half_lat_deg + edge_lat_deg;
    square.safe_max_lat = square.center.lat + half_lat_deg - edge_lat_deg;
    return square;
}

static const BattlefieldSquare& GetBattlefieldSquare() {
    static const BattlefieldSquare square = BuildBattlefieldSquare();
    return square;
}

static GeoPoint ClampPointToBattlefield(double longitude, double latitude) {
    const BattlefieldSquare& square = GetBattlefieldSquare();
    return GeoPoint{
        std::clamp(longitude, square.safe_min_lon, square.safe_max_lon),
        std::clamp(latitude, square.safe_min_lat, square.safe_max_lat)};
}

static void ConfigurePlaneFrom7D(
    PlaneState_S& p,
    const Unit7D& in,
    int team,
    int plane_id,
    double longitude,
    double latitude,
    double yaw_deg,
    double altitude,
    int limit_index) {
    std::memset(&p, 0, sizeof(PlaneState_S));
    p.init(static_cast<char>(limit_index));
    const GeoPoint safe_start = ClampPointToBattlefield(longitude, latitude);

    p._planeID = plane_id;
    p._team = team;
    p._targetID = -1;
    p._equipmentType = FIGHTER_J15;
    p._isAlive = in.alive > 0.5 ? 1 : 0;

    p._longitude = safe_start.lon;
    p._latitude = safe_start.lat;
    p._altitude = altitude;
    p._yaw = yaw_deg;
    p._roll = 0.0;
    p._pitch = 0.0;

    // Keep Source defaults from PlaneState_S::init(limitIndex), then only
    // override the fields that are explicitly carried by the 7D contract.
    p.TAS = std::max(0.0, in.speed);
    p._throttle = 40.0;
    p._missileCount = std::max(0, std::min(Max_Missile_Count, static_cast<int>(std::round(in.missile))));

    if (in.sensor > 0.0) {
        const double radar_range_m = std::max(1000.0, in.sensor * 1000.0);
        p._radar_range = radar_range_m;
        p._radar_view_range = radar_range_m;
        p._radar_all_range = radar_range_m;
    }

    p._radar_hBeamWidth = 90.0;
    p._radar_vBeamWidth = 60.0;
    p._radar_vBeamWidth_upper = 90.0;
    p._radar_vBeamWidth_below = 90.0;
    p._radar_all_hBeamWidth = 360.0;
    p._radar_all_vBeamWidth = 180.0;
    p._radarState = RadarState_E::Scan;
    p._radarMode = RadarMode_E::RM_Normal;
    std::snprintf(p._pilot, sizeof(p._pilot), "%d", plane_id);
}

static void RestorePlaneContractState(
    PlaneState_S& p,
    const Unit7D& in,
    double longitude,
    double latitude) {
    const GeoPoint safe_start = ClampPointToBattlefield(longitude, latitude);
    p._isAlive = in.alive > 0.5 ? 1 : 0;
    p._longitude = safe_start.lon;
    p._latitude = safe_start.lat;
    p.TAS = std::max(0.0, in.speed);
    p._throttle = 40.0;
    p._targetID = -1;
    p._isShoot = false;
    p._missileCount = std::max(0, std::min(Max_Missile_Count, static_cast<int>(std::round(in.missile))));

    if (in.sensor > 0.0) {
        const double radar_range_m = std::max(1000.0, in.sensor * 1000.0);
        p._radar_range = radar_range_m;
        p._radar_view_range = radar_range_m;
        p._radar_all_range = radar_range_m;
    }

    p._radar_hBeamWidth = 90.0;
    p._radar_vBeamWidth = 60.0;
    p._radar_vBeamWidth_upper = 90.0;
    p._radar_vBeamWidth_below = 90.0;
    p._radar_all_hBeamWidth = 360.0;
    p._radar_all_vBeamWidth = 180.0;
    p._radarState = RadarState_E::Scan;
    p._radarMode = RadarMode_E::RM_Normal;
}

static void SyncFromEngine(PlaneState_S* planes, int count) {
    for (int i = 0; i < count; ++i) {
        const PlaneState_S* latest = GetPlaneState(planes[i]._planeID); // Bottom API: state pull.
        if (latest != nullptr) {
            std::memcpy(&planes[i], latest, sizeof(PlaneState_S));
        }
    }
}

static int CountAliveByTeam(const PlaneState_S* planes, int count, int team) {
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == team && planes[i]._isAlive > 0) {
            ++alive;
        }
    }
    return alive;
}

static GeoPoint GetBaseAnchorForTeam(int team) {
    if (team == 1) {
        return GeoPoint{kRedBaseLongitude, kRedBaseLatitude};
    }
    return GeoPoint{kBlueBaseLongitude, kBlueBaseLatitude};
}

static GeoPoint ComputeTeamAnchorFromPlanes(const PlaneState_S* planes, int count, int team, double default_lon, double default_lat) {
    double lon_sum = 0.0;
    double lat_sum = 0.0;
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team != team || planes[i]._isAlive <= 0) {
            continue;
        }
        lon_sum += planes[i]._longitude;
        lat_sum += planes[i]._latitude;
        ++alive;
    }

    if (alive == 0) {
        return GeoPoint{default_lon, default_lat};
    }
    return GeoPoint{
        lon_sum / static_cast<double>(alive),
        lat_sum / static_cast<double>(alive)};
}

static GeoPoint ComputeAnchorForPlaneIds(
    const PlaneState_S* planes,
    int count,
    const std::vector<int>& plane_ids,
    double default_lon,
    double default_lat) {
    double lon_sum = 0.0;
    double lat_sum = 0.0;
    int alive = 0;
    for (int plane_id : plane_ids) {
        const PlaneState_S* plane = nullptr;
        for (int i = 0; i < count; ++i) {
            if (planes[i]._planeID == plane_id) {
                plane = &planes[i];
                break;
            }
        }
        if (plane == nullptr || plane->_isAlive <= 0) {
            continue;
        }
        lon_sum += plane->_longitude;
        lat_sum += plane->_latitude;
        ++alive;
    }

    if (alive == 0) {
        return GeoPoint{default_lon, default_lat};
    }
    return GeoPoint{
        lon_sum / static_cast<double>(alive),
        lat_sum / static_cast<double>(alive)};
}

static PlaneState_S* FindPlaneById(PlaneState_S* planes, int count, int plane_id) {
    for (int i = 0; i < count; ++i) {
        if (planes[i]._planeID == plane_id) {
            return &planes[i];
        }
    }
    return nullptr;
}

static const PlaneState_S* FindPlaneById(const PlaneState_S* planes, int count, int plane_id) {
    for (int i = 0; i < count; ++i) {
        if (planes[i]._planeID == plane_id) {
            return &planes[i];
        }
    }
    return nullptr;
}

static void ResetPlaneCommand(PlaneState_S& plane) {
    plane._isShoot = false;
    plane._targetID = -1;
    plane._radarMode = RadarMode_E::RM_Normal;
    plane._radarState = RadarState_E::Scan;
}

static bool IsPlaneAliveById(const PlaneState_S* planes, int count, int plane_id) {
    const PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
    return plane != nullptr && plane->_isAlive > 0;
}

static std::string MakeSafeFileStem(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "task";
    }
    return out;
}

static int CountAliveUnits(const std::vector<Unit2D>& units) {
    int alive = 0;
    for (const auto& unit : units) {
        if (unit.final_alive > 0.5) {
            ++alive;
        }
    }
    return alive;
}

static int SumRemainingMissiles(const std::vector<Unit2D>& units) {
    int missiles = 0;
    for (const auto& unit : units) {
        missiles += static_cast<int>(std::round(std::max(0.0, unit.final_missile)));
    }
    return missiles;
}

static int CountCandidateTasks(const std::string& input_path, int max_tasks) {
    std::ifstream fin(input_path);
    if (!fin.is_open()) {
        return 0;
    }

    int count = 0;
    std::string line;
    while (count < max_tasks && std::getline(fin, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}

static bool TryParseLooseInt(const std::string& text, int* value) {
    if (value == nullptr) {
        return false;
    }

    try {
        size_t parsed_chars = 0;
        const int parsed = std::stoi(text, &parsed_chars);
        if (parsed_chars == text.size()) {
            *value = parsed;
            return true;
        }
    } catch (const std::exception&) {
    }

    std::string current_run;
    std::string last_run;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch)) || (ch == '-' && current_run.empty())) {
            current_run.push_back(ch);
            continue;
        }
        if (!current_run.empty()) {
            last_run = current_run;
            current_run.clear();
        }
    }
    if (!current_run.empty()) {
        last_run = current_run;
    }

    if (last_run.empty() || last_run == "-") {
        return false;
    }

    try {
        *value = std::stoi(last_run);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

static bool TryExtractTacticId(const json& payload, int* tactic_id) {
    if (tactic_id == nullptr) {
        return false;
    }
    if (payload.is_number_integer()) {
        *tactic_id = payload.get<int>();
        return true;
    }
    if (payload.is_number_float()) {
        *tactic_id = static_cast<int>(std::round(payload.get<double>()));
        return true;
    }
    if (payload.is_string()) {
        return TryParseLooseInt(payload.get<std::string>(), tactic_id);
    }
    if (!payload.is_object()) {
        return false;
    }

    if (payload.contains("params") && payload["params"].is_object()) {
        const json& params = payload["params"];
        if (params.contains("tactic_id") && TryExtractTacticId(params["tactic_id"], tactic_id)) {
            return true;
        }
    }
    if (payload.contains("tactic_id") && TryExtractTacticId(payload["tactic_id"], tactic_id)) {
        return true;
    }
    if (payload.contains("id") && TryExtractTacticId(payload["id"], tactic_id)) {
        return true;
    }
    return false;
}

static json BuildLegacyTacticCondition(int tactic_id) {
    return json{
        {"source", "legacy_tactic_id"},
        {"family", "legacy_shared_tactic"},
        {"id", "tactic_" + std::to_string(tactic_id)},
        {"params", {{"tactic_id", tactic_id}}}};
}

static json NormalizeTaskTacticCondition(
    const json& task,
    const char* field_name,
    int legacy_tactic_id) {
    if (!task.contains(field_name) || !task[field_name].is_object()) {
        return BuildLegacyTacticCondition(legacy_tactic_id);
    }

    json normalized = task[field_name];
    int resolved_tactic_id = legacy_tactic_id;
    if (!TryExtractTacticId(normalized, &resolved_tactic_id)) {
        resolved_tactic_id = legacy_tactic_id;
    }

    if (!normalized.contains("source")) {
        normalized["source"] = "task_config";
    } else if (!normalized["source"].is_string()) {
        normalized["source"] = normalized["source"].dump();
    }

    if (!normalized.contains("family")) {
        normalized["family"] = "rule";
    } else if (!normalized["family"].is_string()) {
        normalized["family"] = normalized["family"].dump();
    }

    if (!normalized.contains("id")) {
        normalized["id"] = "tactic_" + std::to_string(resolved_tactic_id);
    } else if (!normalized["id"].is_string()) {
        normalized["id"] = normalized["id"].dump();
    }

    json params = normalized.value("params", json::object());
    if (!params.is_object()) {
        params = json::object();
    }
    params["tactic_id"] = resolved_tactic_id;
    normalized["params"] = params;
    return normalized;
}

static int ResolveTacticId(const json& tactic_condition, int fallback_tactic_id) {
    int tactic_id = fallback_tactic_id;
    if (TryExtractTacticId(tactic_condition, &tactic_id)) {
        return tactic_id;
    }
    return fallback_tactic_id;
}

static json BuildRolloutTacticCondition(
    const json& tactic_condition,
    const char* side,
    int resolved_tactic_id) {
    json compact = json::object();
    compact["side"] = side;

    if (tactic_condition.contains("source")) {
        compact["source"] = tactic_condition["source"];
    } else {
        compact["source"] = "task_config";
    }

    compact["family"] = tactic_condition.value("family", "rule");
    if (tactic_condition.contains("id")) {
        compact["id"] = tactic_condition["id"];
    } else {
        compact["id"] = resolved_tactic_id;
    }

    json params = tactic_condition.value("params", json::object());
    if (!params.is_object()) {
        params = json::object();
    }
    params["tactic_id"] = resolved_tactic_id;
    compact["params"] = params;
    return compact;
}

static std::string JsonValueToStableString(const json& value, const char* fallback) {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<int>());
    }
    if (value.is_number_float()) {
        std::ostringstream oss;
        oss << value.get<double>();
        return oss.str();
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    return std::string(fallback);
}

static json NormalizeTaskSamplingMeta(
    const json& task,
    const json& red_tactic_condition,
    const json& blue_tactic_condition,
    size_t red_unit_count,
    size_t blue_unit_count) {
    json normalized = task.value("sampling_meta", json::object());
    if (!normalized.is_object()) {
        normalized = json::object();
    }

    const std::string red_family = JsonValueToStableString(
        red_tactic_condition.value("family", json("rule")),
        "rule");
    const std::string blue_family = JsonValueToStableString(
        blue_tactic_condition.value("family", json("rule")),
        "rule");
    const std::string red_id = JsonValueToStableString(
        red_tactic_condition.value("id", json("unknown_red")),
        "unknown_red");
    const std::string blue_id = JsonValueToStableString(
        blue_tactic_condition.value("id", json("unknown_blue")),
        "unknown_blue");
    const std::string force_size_key = std::to_string(red_unit_count) + "v" + std::to_string(blue_unit_count);
    const std::string tactic_pair_key = red_family + "-" + blue_family;
    const std::string tactic_combo_key = red_id + "__" + blue_id;
    const std::string fallback_scenario_key = force_size_key + "__" + tactic_combo_key;

    if (!normalized.contains("sampling_plan")) {
        normalized["sampling_plan"] = "rollout_fallback_v1";
    }
    normalized["tactic_pair_key"] = JsonValueToStableString(
        normalized.value("tactic_pair_key", json(tactic_pair_key)),
        tactic_pair_key.c_str());
    normalized["tactic_combo_key"] = JsonValueToStableString(
        normalized.value("tactic_combo_key", json(tactic_combo_key)),
        tactic_combo_key.c_str());
    normalized["red_tactic_family"] = JsonValueToStableString(
        normalized.value("red_tactic_family", json(red_family)),
        red_family.c_str());
    normalized["blue_tactic_family"] = JsonValueToStableString(
        normalized.value("blue_tactic_family", json(blue_family)),
        blue_family.c_str());
    normalized["red_tactic_id"] = red_tactic_condition["params"].value("tactic_id", 0);
    normalized["blue_tactic_id"] = blue_tactic_condition["params"].value("tactic_id", 0);
    normalized["force_size_key"] = JsonValueToStableString(
        normalized.value("force_size_key", json(force_size_key)),
        force_size_key.c_str());
    normalized["red_force_size"] = normalized.value("red_force_size", static_cast<int>(red_unit_count));
    normalized["blue_force_size"] = normalized.value("blue_force_size", static_cast<int>(blue_unit_count));
    normalized["red_attr_bucket"] = JsonValueToStableString(
        normalized.value("red_attr_bucket", json("unknown")),
        "unknown");
    normalized["blue_attr_bucket"] = JsonValueToStableString(
        normalized.value("blue_attr_bucket", json("unknown")),
        "unknown");
    normalized["scenario_key"] = JsonValueToStableString(
        normalized.value("scenario_key", json(fallback_scenario_key)),
        fallback_scenario_key.c_str());

    const std::string split_group_key = JsonValueToStableString(
        task.value(
            "split_group_key",
            normalized.value("split_group_key", normalized["scenario_key"])),
        JsonValueToStableString(normalized["scenario_key"], fallback_scenario_key.c_str()).c_str());
    normalized["split_group_key"] = split_group_key;
    return normalized;
}

static json BuildUnitSnapshot(const PlaneState_S& plane, const Unit7D& seed_unit, const char* side) {
    return json{
        {"unit_id", std::string(side) + "_" + std::to_string(plane._planeID)},
        {"type_id", seed_unit.type_id},
        {"alive", plane._isAlive > 0 ? 1 : 0},
        {"missile_count", std::max(0, plane._missileCount)},
        {"lon", plane._longitude},
        {"lat", plane._latitude},
        {"alt_m", plane._altitude},
        {"speed_mps", plane.TAS},
        {"heading_deg", NormalizeHeadingDegrees(plane._yaw)}};
}

static json BuildStateSnapshot(
    const std::vector<PlaneState_S>& planes,
    int red_count,
    int blue_count,
    const std::vector<Unit7D>& red_units,
    const std::vector<Unit7D>& blue_units) {
    json state;
    state["red_units"] = json::array();
    state["blue_units"] = json::array();

    for (int i = 0; i < red_count; ++i) {
        state["red_units"].push_back(BuildUnitSnapshot(
            planes[i],
            red_units[static_cast<size_t>(i)],
            "red"));
    }
    for (int i = 0; i < blue_count; ++i) {
        const int pidx = red_count + i;
        state["blue_units"].push_back(BuildUnitSnapshot(
            planes[pidx],
            blue_units[static_cast<size_t>(i)],
            "blue"));
    }
    return state;
}

static TerminationStatus DetectTermination(
    int red_alive,
    int blue_alive,
    bool red_objective_complete,
    bool blue_objective_complete,
    int step_plus_one) {
    if (red_alive == 0) {
        return TerminationStatus{true, "red_eliminated"};
    }
    if (blue_alive == 0) {
        return TerminationStatus{true, "blue_eliminated"};
    }
    if (red_objective_complete || blue_objective_complete) {
        return TerminationStatus{true, "objective_complete"};
    }
    if (step_plus_one >= kMissionSafetyMaxSteps) {
        return TerminationStatus{true, "safety_limit"};
    }
    return TerminationStatus{};
}

static json BuildRolloutEventJson(const RolloutEventAccumulator& event_acc) {
    return json{
        {"red_fire_count_delta", event_acc.red_fire_count_delta},
        {"blue_fire_count_delta", event_acc.blue_fire_count_delta},
        {"red_kill_delta", event_acc.red_kill_delta},
        {"blue_kill_delta", event_acc.blue_kill_delta},
        {"red_dodge_trigger_count", event_acc.red_dodge_trigger_count},
        {"blue_dodge_trigger_count", event_acc.blue_dodge_trigger_count},
        {"red_first_contact_flag", event_acc.red_first_contact_flag},
        {"blue_first_contact_flag", event_acc.blue_first_contact_flag},
        {"red_contact_flag", event_acc.red_contact_flag},
        {"blue_contact_flag", event_acc.blue_contact_flag},
        {"red_first_fire_flag", event_acc.red_first_fire_flag},
        {"blue_first_fire_flag", event_acc.blue_first_fire_flag},
        {"red_warning_flag", event_acc.red_warning_flag},
        {"blue_warning_flag", event_acc.blue_warning_flag},
        {"red_retarget_flag", event_acc.red_retarget_flag},
        {"blue_retarget_flag", event_acc.blue_retarget_flag},
        {"red_first_kill_flag", event_acc.red_first_kill_flag},
        {"blue_first_kill_flag", event_acc.blue_first_kill_flag},
        {"red_objective_complete_flag", event_acc.red_objective_complete_flag},
        {"blue_objective_complete_flag", event_acc.blue_objective_complete_flag},
        {"termination_flag", event_acc.termination_flag}};
}

static std::pair<double, double> ComputeRewardStep(
    const RolloutEventAccumulator& event_acc) {
    const double red_reward =
        1.00 * static_cast<double>(event_acc.red_kill_delta) -
        1.00 * static_cast<double>(event_acc.blue_kill_delta) +
        0.25 * static_cast<double>(event_acc.red_first_contact_flag ? 1 : 0) +
        0.10 * static_cast<double>(event_acc.red_contact_flag ? 1 : 0) +
        0.25 * static_cast<double>(event_acc.red_first_fire_flag ? 1 : 0) +
        0.05 * static_cast<double>(event_acc.red_fire_count_delta) +
        0.50 * static_cast<double>(event_acc.red_first_kill_flag ? 1 : 0) -
        0.05 * static_cast<double>(event_acc.red_warning_flag ? 1 : 0) -
        0.02 * static_cast<double>(event_acc.red_dodge_trigger_count) +
        1.50 * static_cast<double>(event_acc.red_objective_complete_flag ? 1 : 0) -
        1.50 * static_cast<double>(event_acc.blue_objective_complete_flag ? 1 : 0);
    const double blue_reward =
        1.00 * static_cast<double>(event_acc.blue_kill_delta) -
        1.00 * static_cast<double>(event_acc.red_kill_delta) +
        0.25 * static_cast<double>(event_acc.blue_first_contact_flag ? 1 : 0) +
        0.10 * static_cast<double>(event_acc.blue_contact_flag ? 1 : 0) +
        0.25 * static_cast<double>(event_acc.blue_first_fire_flag ? 1 : 0) +
        0.05 * static_cast<double>(event_acc.blue_fire_count_delta) +
        0.50 * static_cast<double>(event_acc.blue_first_kill_flag ? 1 : 0) -
        0.05 * static_cast<double>(event_acc.blue_warning_flag ? 1 : 0) -
        0.02 * static_cast<double>(event_acc.blue_dodge_trigger_count) +
        1.50 * static_cast<double>(event_acc.blue_objective_complete_flag ? 1 : 0) -
        1.50 * static_cast<double>(event_acc.red_objective_complete_flag ? 1 : 0);
    return {red_reward, blue_reward};
}

static json BuildRewardJson(double red_reward, double blue_reward) {
    return json{{"red", red_reward}, {"blue", blue_reward}};
}

static json BuildEpisodeOutcomeJson(const BattleOutcome& battle) {
    return json{
        {"red_win", battle.red_win},
        {"elapsed_steps", battle.elapsed_steps},
        {"termination_reason", battle.termination_reason},
        {"red_alive_final", CountAliveUnits(battle.red_final)},
        {"blue_alive_final", CountAliveUnits(battle.blue_final)},
        {"red_missile_final", SumRemainingMissiles(battle.red_final)},
        {"blue_missile_final", SumRemainingMissiles(battle.blue_final)}};
}

static BattleOutcome RunTactic(
    const std::vector<Unit7D>& red_units,
    const std::vector<Unit7D>& blue_units,
    int red_tactic_id,
    int blue_tactic_id,
    const json& red_tactic_condition,
    const json& blue_tactic_condition,
    const json& sampling_meta,
    bool write_replay,
    int episode_index,
    const std::string& task_id,
    int task_number,
    int total_tasks) {
    BattleOutcome out;
    out.red_final.resize(red_units.size(), Unit2D{0.0, 0.0});
    out.blue_final.resize(blue_units.size(), Unit2D{0.0, 0.0});

    // Keep contract output size stable even if input is not exactly 2v1.
    for (size_t i = 0; i < red_units.size(); ++i) {
        out.red_final[i] = Unit2D{std::max(0.0, red_units[i].missile), Clamp01(red_units[i].alive)};
    }
    for (size_t i = 0; i < blue_units.size(); ++i) {
        out.blue_final[i] = Unit2D{std::max(0.0, blue_units[i].missile), Clamp01(blue_units[i].alive)};
    }

    const int kMaxSidePlanes = ZhanShu::kDefaultPlaneCount / 2;
    const int red_count = static_cast<int>(std::min<size_t>(kMaxSidePlanes, red_units.size()));
    const int blue_count = static_cast<int>(std::min<size_t>(kMaxSidePlanes, blue_units.size()));
    const int plane_count = red_count + blue_count;
    if (plane_count == 0) {
        return out;
    }

    std::vector<PlaneState_S> planes(static_cast<size_t>(plane_count));
    std::memset(planes.data(), 0, sizeof(PlaneState_S) * planes.size());

    int idx = 0;
    for (int i = 0; i < red_count; ++i, ++idx) {
        ConfigurePlaneFrom7D(
            planes[idx],
            red_units[i],
            1,
            10012 + i,
            red_units[i].lon,
            red_units[i].lat,
            90.0,
            5000.0 - i * 50.0,
            MAV_RED_2);
    }
    for (int i = 0; i < blue_count; ++i, ++idx) {
        ConfigurePlaneFrom7D(
            planes[idx],
            blue_units[i],
            2,
            20012 + i,
            blue_units[i].lon,
            blue_units[i].lat,
            270.0,
            5000.0 - i * 50.0,
            MAV_BLUE_1);
    }

    std::string acmi_path;
    const char* init_acmi_path = nullptr;
    if (write_replay) {
        std::filesystem::create_directories(kReplayDirectory);
        std::ostringstream oss;
        oss << "episode_" << (episode_index + 1) << "_" << MakeSafeFileStem(task_id) << ".acmi";
        acmi_path = (std::filesystem::path(kReplayDirectory) / oss.str()).string();
        init_acmi_path = acmi_path.c_str(); // Bottom API: optional local ACMI output path.
    }

    // Bottom API: environment init.
    if (!InitEnv(init_acmi_path, planes.data(), static_cast<uint32_t>(plane_count))) {
        out.termination_reason = "InitEnv failed";
        return out;
    }

    if (write_replay) {
        const BattlefieldSquare& battlefield = GetBattlefieldSquare();
        AddBoundary(
            1,
            battlefield.top_left_lon,
            battlefield.top_left_lat,
            7000.0,
            battlefield.side_length_m,
            battlefield.side_length_m,
            20000.0,
            Color_E::Violet);
    }

    // InitEnv resets missile count to Max_Missile_Count, restore from 7D contract.
    for (int i = 0; i < red_count; ++i) {
        RestorePlaneContractState(planes[i], red_units[i], red_units[i].lon, red_units[i].lat);
        if (red_units[i].alive <= 0.5) {
            killPlane(planes[i]._planeID);
            planes[i]._isAlive = 0;
        }
    }
    for (int i = 0; i < blue_count; ++i) {
        const int pidx = red_count + i;
        RestorePlaneContractState(planes[pidx], blue_units[i], blue_units[i].lon, blue_units[i].lat);
        if (blue_units[i].alive <= 0.5) {
            killPlane(planes[pidx]._planeID);
            planes[pidx]._isAlive = 0;
        }
    }

    const double dt = kSimulationStepSeconds;
    int red_alive = 0;
    int blue_alive = 0;
    const GeoPoint red_base_anchor = GetBaseAnchorForTeam(1);
    const GeoPoint blue_base_anchor = GetBaseAnchorForTeam(2);
    const GeoPoint red_scene_anchor_seed = ComputeTeamAnchorFromPlanes(
        planes.data(),
        plane_count,
        1,
        red_base_anchor.lon,
        red_base_anchor.lat);
    const GeoPoint blue_scene_anchor_seed = ComputeTeamAnchorFromPlanes(
        planes.data(),
        plane_count,
        2,
        blue_base_anchor.lon,
        blue_base_anchor.lat);
    EncounterBatchTactics::TeamTacticController red_tactic(
        1,
        red_tactic_id,
        EncounterBatchTactics::GeoPoint{red_scene_anchor_seed.lon, red_scene_anchor_seed.lat},
        "red");
    EncounterBatchTactics::TeamTacticController blue_tactic(
        2,
        blue_tactic_id,
        EncounterBatchTactics::GeoPoint{blue_scene_anchor_seed.lon, blue_scene_anchor_seed.lat},
        "blue");
    const json compact_red_tactic_condition = BuildRolloutTacticCondition(
        red_tactic_condition,
        "red",
        red_tactic_id);
    const json compact_blue_tactic_condition = BuildRolloutTacticCondition(
        blue_tactic_condition,
        "blue",
        blue_tactic_id);
    red_tactic.Initialize(planes.data(), plane_count);
    blue_tactic.Initialize(planes.data(), plane_count);
    bool should_sync_from_engine = false;
    bool reached_terminal_state = false;
    std::vector<json> rollout_buffer;
    json export_interval_start_state = BuildStateSnapshot(
        planes,
        red_count,
        blue_count,
        red_units,
        blue_units);
    const auto initial_red_telemetry = red_tactic.GetTelemetry(planes.data(), plane_count);
    const auto initial_blue_telemetry = blue_tactic.GetTelemetry(planes.data(), plane_count);
    EpisodeEventState episode_event_state;
    episode_event_state.red_contact_seen = initial_red_telemetry.contact_plane_count > 0;
    episode_event_state.blue_contact_seen = initial_blue_telemetry.contact_plane_count > 0;
    episode_event_state.red_fire_seen = initial_red_telemetry.fired_total > 0;
    episode_event_state.blue_fire_seen = initial_blue_telemetry.fired_total > 0;
    auto previous_red_telemetry = initial_red_telemetry;
    auto previous_blue_telemetry = initial_blue_telemetry;
    RolloutEventAccumulator interval_event;

    red_alive = CountAliveByTeam(planes.data(), plane_count, 1);
    blue_alive = CountAliveByTeam(planes.data(), plane_count, 2);
    const bool red_initial_objective_complete = red_tactic.UpdateObjective(planes.data(), plane_count);
    const bool blue_initial_objective_complete = blue_tactic.UpdateObjective(planes.data(), plane_count);
    bool previous_red_objective_complete = red_initial_objective_complete;
    bool previous_blue_objective_complete = blue_initial_objective_complete;
    const TerminationStatus initial_term = DetectTermination(
        red_alive,
        blue_alive,
        red_initial_objective_complete,
        blue_initial_objective_complete,
        0);
    if (initial_term.done) {
        out.elapsed_steps = 0;
        out.termination_reason = initial_term.reason;
        reached_terminal_state = true;
    }

    std::cout
        << "[TASK " << task_number << "/" << total_tasks << "] mission_plan"
        << " | red_target=" << initial_red_telemetry.target_id
        << " | blue_target=" << initial_blue_telemetry.target_id
        << " | red_attackers=" << initial_red_telemetry.primary_attacker_count
        << " | blue_attackers=" << initial_blue_telemetry.primary_attacker_count
        << " | red_decoys=" << initial_red_telemetry.decoy_count
        << " | blue_decoys=" << initial_blue_telemetry.decoy_count
        << " | red_salvos=" << initial_red_telemetry.planned_salvos
        << " | blue_salvos=" << initial_blue_telemetry.planned_salvos
        << " | red_side=" << ((initial_red_telemetry.side == ZhanShu::SurroundSide::West) ? "west" : "east")
        << " | blue_side=" << ((initial_blue_telemetry.side == ZhanShu::SurroundSide::West) ? "west" : "east")
        << std::endl;

    for (int step = 0; step < kMissionSafetyMaxSteps && !reached_terminal_state; ++step) {
        if (should_sync_from_engine) {
            SyncFromEngine(planes.data(), plane_count);
        } else {
            should_sync_from_engine = true;
        }

        red_alive = CountAliveByTeam(planes.data(), plane_count, 1);
        blue_alive = CountAliveByTeam(planes.data(), plane_count, 2);
        red_tactic.RefreshIfNeeded(planes.data(), plane_count, step, task_number, total_tasks);
        blue_tactic.RefreshIfNeeded(planes.data(), plane_count, step, task_number, total_tasks);

        const auto red_telemetry = red_tactic.GetTelemetry(planes.data(), plane_count);
        const auto blue_telemetry = blue_tactic.GetTelemetry(planes.data(), plane_count);
        const bool red_target_alive = red_telemetry.target_id != -1 && IsPlaneAliveById(planes.data(), plane_count, red_telemetry.target_id);
        const bool blue_target_alive = blue_telemetry.target_id != -1 && IsPlaneAliveById(planes.data(), plane_count, blue_telemetry.target_id);
        const bool red_objective_complete = red_tactic.objective_complete();
        const bool blue_objective_complete = blue_tactic.objective_complete();

        if (step == 0 || ((step + 1) % kMissionLogIntervalSteps) == 0) {
            std::cout
                << "[TASK " << task_number << "/" << total_tasks << "][ROUND " << (step + 1) << "]"
                << " | sim_time_s=" << ((step + 1) * dt)
                << " | red_alive=" << red_alive
                << " | blue_alive=" << blue_alive
                << " | red_target_alive=" << (red_target_alive ? "yes" : "no")
                << " | blue_target_alive=" << (blue_target_alive ? "yes" : "no")
                << " | red_shots=" << red_telemetry.fired_total
                << " | blue_shots=" << blue_telemetry.fired_total
                << " | red_attackers=" << red_telemetry.primary_attacker_count
                << " | blue_attackers=" << blue_telemetry.primary_attacker_count
                << " | red_decoys=" << red_telemetry.decoy_count
                << " | blue_decoys=" << blue_telemetry.decoy_count
                << " | red_side=" << ((red_telemetry.side == ZhanShu::SurroundSide::West) ? "west" : "east")
                << " | blue_side=" << ((blue_telemetry.side == ZhanShu::SurroundSide::West) ? "west" : "east")
                << " | red_objective=" << (red_objective_complete ? "done" : "running")
                << " | blue_objective=" << (blue_objective_complete ? "done" : "running")
                << std::endl;
        }

        for (int i = 0; i < plane_count; ++i) {
            if (planes[i]._isAlive <= 0) {
                continue;
            }
            ResetPlaneCommand(planes[i]);
        }

        red_tactic.Apply(planes.data(), plane_count, step, dt);
        blue_tactic.Apply(planes.data(), plane_count, step, dt);

        // Bottom API: physics step.
        EnvStep(dt, planes.data());
        SyncFromEngine(planes.data(), plane_count);

        const json export_interval_end_state = BuildStateSnapshot(
            planes,
            red_count,
            blue_count,
            red_units,
            blue_units);

        const int red_alive_after = CountAliveByTeam(planes.data(), plane_count, 1);
        const int blue_alive_after = CountAliveByTeam(planes.data(), plane_count, 2);
        const bool red_objective_after = red_tactic.UpdateObjective(planes.data(), plane_count);
        const bool blue_objective_after = blue_tactic.UpdateObjective(planes.data(), plane_count);
        const auto red_telemetry_after = red_tactic.GetTelemetry(planes.data(), plane_count);
        const auto blue_telemetry_after = blue_tactic.GetTelemetry(planes.data(), plane_count);

        const int red_fire_step_delta = std::max(0, red_telemetry_after.fired_total - previous_red_telemetry.fired_total);
        const int blue_fire_step_delta = std::max(0, blue_telemetry_after.fired_total - previous_blue_telemetry.fired_total);
        const int red_kill_step_delta = std::max(0, blue_alive - blue_alive_after);
        const int blue_kill_step_delta = std::max(0, red_alive - red_alive_after);
        const int red_dodge_step_delta = std::max(
            0,
            red_telemetry_after.dodge_trigger_total - previous_red_telemetry.dodge_trigger_total);
        const int blue_dodge_step_delta = std::max(
            0,
            blue_telemetry_after.dodge_trigger_total - previous_blue_telemetry.dodge_trigger_total);
        const bool red_contact_active = red_telemetry_after.contact_plane_count > 0;
        const bool blue_contact_active = blue_telemetry_after.contact_plane_count > 0;
        const bool red_warning_active =
            red_telemetry_after.warning_plane_count > 0 || red_telemetry_after.warning_max_count > 0;
        const bool blue_warning_active =
            blue_telemetry_after.warning_plane_count > 0 || blue_telemetry_after.warning_max_count > 0;
        const bool red_retarget_step = red_telemetry_after.retarget_total > previous_red_telemetry.retarget_total;
        const bool blue_retarget_step = blue_telemetry_after.retarget_total > previous_blue_telemetry.retarget_total;

        interval_event.red_fire_count_delta += red_fire_step_delta;
        interval_event.blue_fire_count_delta += blue_fire_step_delta;
        interval_event.red_kill_delta += red_kill_step_delta;
        interval_event.blue_kill_delta += blue_kill_step_delta;
        interval_event.red_dodge_trigger_count += red_dodge_step_delta;
        interval_event.blue_dodge_trigger_count += blue_dodge_step_delta;
        interval_event.red_contact_flag = interval_event.red_contact_flag || red_contact_active;
        interval_event.blue_contact_flag = interval_event.blue_contact_flag || blue_contact_active;
        interval_event.red_warning_flag = interval_event.red_warning_flag || red_warning_active;
        interval_event.blue_warning_flag = interval_event.blue_warning_flag || blue_warning_active;
        interval_event.red_retarget_flag = interval_event.red_retarget_flag || red_retarget_step;
        interval_event.blue_retarget_flag = interval_event.blue_retarget_flag || blue_retarget_step;
        interval_event.red_objective_complete_flag =
            interval_event.red_objective_complete_flag ||
            (red_objective_after && !previous_red_objective_complete);
        interval_event.blue_objective_complete_flag =
            interval_event.blue_objective_complete_flag ||
            (blue_objective_after && !previous_blue_objective_complete);

        if (!episode_event_state.red_contact_seen && red_contact_active) {
            interval_event.red_first_contact_flag = true;
            episode_event_state.red_contact_seen = true;
        }
        if (!episode_event_state.blue_contact_seen && blue_contact_active) {
            interval_event.blue_first_contact_flag = true;
            episode_event_state.blue_contact_seen = true;
        }
        if (!episode_event_state.red_fire_seen && red_fire_step_delta > 0) {
            interval_event.red_first_fire_flag = true;
            episode_event_state.red_fire_seen = true;
        } else if (red_fire_step_delta > 0) {
            episode_event_state.red_fire_seen = true;
        }
        if (!episode_event_state.blue_fire_seen && blue_fire_step_delta > 0) {
            interval_event.blue_first_fire_flag = true;
            episode_event_state.blue_fire_seen = true;
        } else if (blue_fire_step_delta > 0) {
            episode_event_state.blue_fire_seen = true;
        }
        if (!episode_event_state.red_kill_seen && red_kill_step_delta > 0) {
            interval_event.red_first_kill_flag = true;
            episode_event_state.red_kill_seen = true;
        } else if (red_kill_step_delta > 0) {
            episode_event_state.red_kill_seen = true;
        }
        if (!episode_event_state.blue_kill_seen && blue_kill_step_delta > 0) {
            interval_event.blue_first_kill_flag = true;
            episode_event_state.blue_kill_seen = true;
        } else if (blue_kill_step_delta > 0) {
            episode_event_state.blue_kill_seen = true;
        }

        const TerminationStatus term = DetectTermination(
            red_alive_after,
            blue_alive_after,
            red_objective_after,
            blue_objective_after,
            step + 1);
        const bool on_export_stride = (((step + 1) % kRolloutExportStrideSteps) == 0);
        interval_event.termination_flag = interval_event.termination_flag || term.done;

        if (on_export_stride || term.done) {
            const auto reward_step = ComputeRewardStep(interval_event);
            episode_event_state.red_reward_cumulative += reward_step.first;
            episode_event_state.blue_reward_cumulative += reward_step.second;
            json row;
            row["task_id"] = task_id;
            row["episode_id"] = task_id;
            row["split_group_key"] = sampling_meta.value("split_group_key", task_id);
            row["sampling_meta"] = sampling_meta;
            row["step"] = step + 1;
            row["sim_time_s"] = (step + 1) * dt;
            row["red_tactic_condition"] = compact_red_tactic_condition;
            row["blue_tactic_condition"] = compact_blue_tactic_condition;
            row["state"] = export_interval_start_state;
            row["next_state"] = export_interval_end_state;
            row["done"] = term.done;
            row["termination_reason"] = term.reason;
            row["event"] = BuildRolloutEventJson(interval_event);
            row["reward_step"] = BuildRewardJson(reward_step.first, reward_step.second);
            row["reward_cumulative"] = BuildRewardJson(
                episode_event_state.red_reward_cumulative,
                episode_event_state.blue_reward_cumulative);
            rollout_buffer.push_back(std::move(row));
            export_interval_start_state = export_interval_end_state;
            interval_event = RolloutEventAccumulator{};
        }

        previous_red_telemetry = red_telemetry_after;
        previous_blue_telemetry = blue_telemetry_after;
        previous_red_objective_complete = red_objective_after;
        previous_blue_objective_complete = blue_objective_after;
        red_alive = red_alive_after;
        blue_alive = blue_alive_after;

        if (term.done) {
            out.elapsed_steps = step + 1;
            out.termination_reason = term.reason;
            reached_terminal_state = true;
        }
    }

    if (!reached_terminal_state) {
        out.elapsed_steps = kMissionSafetyMaxSteps;
        out.termination_reason = "safety_limit";
    }

    red_alive = CountAliveByTeam(planes.data(), plane_count, 1);
    blue_alive = CountAliveByTeam(planes.data(), plane_count, 2);
    out.red_objective_complete = red_tactic.objective_complete();
    out.blue_objective_complete = blue_tactic.objective_complete();

    for (int i = 0; i < red_count; ++i) {
        out.red_final[i].final_missile = std::max(0.0, static_cast<double>(planes[i]._missileCount));
        out.red_final[i].final_alive = planes[i]._isAlive > 0 ? 1.0 : 0.0;
    }
    for (int i = 0; i < blue_count; ++i) {
        const int pidx = red_count + i;
        out.blue_final[i].final_missile = std::max(0.0, static_cast<double>(planes[pidx]._missileCount));
        out.blue_final[i].final_alive = planes[pidx]._isAlive > 0 ? 1.0 : 0.0;
    }

    if (out.red_objective_complete && !out.blue_objective_complete) {
        out.red_win = 1;
    } else if (!out.red_objective_complete && out.blue_objective_complete) {
        out.red_win = 0;
    } else if (red_alive > 0 && blue_alive == 0) {
        out.red_win = 1;
    } else if (red_alive == 0 && blue_alive > 0) {
        out.red_win = 0;
    } else {
        const int red_missiles_left = SumRemainingMissiles(out.red_final);
        const int blue_missiles_left = SumRemainingMissiles(out.blue_final);
        if (red_alive != blue_alive) {
            out.red_win = red_alive > blue_alive ? 1 : 0;
        } else {
            out.red_win = red_missiles_left >= blue_missiles_left ? 1 : 0;
        }
    }

    const json episode_outcome = BuildEpisodeOutcomeJson(out);
    for (auto& row : rollout_buffer) {
        row["episode_outcome"] = episode_outcome;
        out.rollout_rows.push_back(std::move(row));
    }

    return out;
}

static std::vector<Unit7D> ParseUnits7D(const json& features) {
    std::vector<Unit7D> out;
    if (!features.is_array()) {
        return out;
    }

    for (const auto& row : features) {
        if (!row.is_array() || row.size() < 7) {
            continue;
        }
        Unit7D u;
        u.type_id = row.at(0).get<double>();
        u.speed = row.at(1).get<double>();
        u.sensor = row.at(2).get<double>();
        u.missile = row.at(3).get<double>();
        u.lon = row.at(4).get<double>();
        u.lat = row.at(5).get<double>();
        u.alive = row.at(6).get<double>();
        out.push_back(u);
    }
    return out;
}

int main() {
    const std::string input_path = "simulation_tasks.jsonl";
    const std::string output_path = "episodes.jsonl";
    const std::string rollout_output_path = "rollouts.jsonl";
    const int kMaxTasks = 1000;
    const int total_tasks = CountCandidateTasks(input_path, kMaxTasks);
    const BattlefieldSquare& battlefield = GetBattlefieldSquare();

    std::ifstream fin(input_path);
    std::ofstream fout(output_path, std::ios::out | std::ios::trunc);
    std::ofstream rollout_fout(rollout_output_path, std::ios::out | std::ios::trunc);
    if (!fin.is_open() || !fout.is_open() || !rollout_fout.is_open()) {
        std::cerr << "[ERROR] cannot open simulation_tasks.jsonl / episodes.jsonl / rollouts.jsonl" << std::endl;
        return 1;
    }

    const auto batch_started = std::chrono::steady_clock::now();
    std::cout
        << "[INFO] batch runner started"
        << " | input=" << input_path
        << " | output=" << output_path
        << " | rollout_output=" << rollout_output_path
        << " | queued_tasks=" << total_tasks
        << " | replay_dir=" << kReplayDirectory
        << " | replay_count=" << ACMI_REPLAY_COUNT
        << " | battlefield_center_lon=" << battlefield.center.lon
        << " | battlefield_center_lat=" << battlefield.center.lat
        << " | battlefield_side_m=" << battlefield.side_length_m
        << std::endl;

    std::string line;
    int count = 0;

    while (count < kMaxTasks && std::getline(fin, line)) {
        if (line.empty()) {
            continue;
        }

        json task;
        try {
            task = json::parse(line);
        } catch (const std::exception& e) {
            std::cerr << "[WARN] skip invalid json line: " << e.what() << std::endl;
            continue;
        }

        const std::string task_id = task.value("task_id", "");
        const int legacy_tactic_id = task.value("tactic_id", 0);
        const json red_tactic_condition = NormalizeTaskTacticCondition(task, "red_tactic_condition", legacy_tactic_id);
        const json blue_tactic_condition = NormalizeTaskTacticCondition(task, "blue_tactic_condition", legacy_tactic_id);
        const int red_tactic_id = ResolveTacticId(red_tactic_condition, legacy_tactic_id);
        const int blue_tactic_id = ResolveTacticId(blue_tactic_condition, legacy_tactic_id);
        const int task_number = count + 1;

        const auto red_units = ParseUnits7D(task["initial_state"]["red_features"]);
        const auto blue_units = ParseUnits7D(task["initial_state"]["blue_features"]);
        const json sampling_meta = NormalizeTaskSamplingMeta(
            task,
            red_tactic_condition,
            blue_tactic_condition,
            red_units.size(),
            blue_units.size());
        const auto task_started = std::chrono::steady_clock::now();

        // ============================================================
        // 【数据解析区】
        // red_units / blue_units 已经是 7D 结构化结果：
        // [type_id, speed, sensor, missile, lon, lat, alive]
        // ============================================================

        const bool write_replay = (ENABLE_ACMI_REPLAY == 1) && (count < ACMI_REPLAY_COUNT);

        std::cout
            << "[TASK " << task_number << "/" << total_tasks << "] starting"
            << " | task_id=" << task_id
            << " | red_tactic=" << red_tactic_id
            << " | blue_tactic=" << blue_tactic_id
            << " | replay=" << (write_replay ? "on" : "off")
            << std::endl;

        // Real tactic execution backed by engine InitEnv/EnvStep APIs.
        const BattleOutcome battle = RunTactic(
            red_units,
            blue_units,
            red_tactic_id,
            blue_tactic_id,
            red_tactic_condition,
            blue_tactic_condition,
            sampling_meta,
            write_replay,
            count,
            task_id,
            task_number,
            total_tasks);
        const auto task_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - task_started);

        // ============================================================
        // 【结果回写区】
        // 封装 task_id + outcome + 2D final_state 写入 episodes.jsonl
        // ============================================================
        json ep;
        ep["task_id"] = task_id;
        ep["outcome"] = {{"red_win", battle.red_win}};
        ep["final_state"]["red_features"] = json::array();
        ep["final_state"]["blue_features"] = json::array();

        for (const auto& u : battle.red_final) {
            ep["final_state"]["red_features"].push_back({u.final_missile, u.final_alive});
        }
        for (const auto& u : battle.blue_final) {
            ep["final_state"]["blue_features"].push_back({u.final_missile, u.final_alive});
        }

        fout << ep.dump() << "\n";
        for (const auto& row : battle.rollout_rows) {
            rollout_fout << row.dump() << "\n";
        }
        ++count;

        std::cout
            << "[TASK " << count << "/" << total_tasks << "] "
            << "completed"
            << " | task_id=" << task_id
            << " | red_tactic=" << red_tactic_id
            << " | blue_tactic=" << blue_tactic_id
            << " | red_units=" << red_units.size()
            << " | blue_units=" << blue_units.size()
            << " | red_win=" << battle.red_win
            << " | red_alive=" << CountAliveUnits(battle.red_final)
            << " | blue_alive=" << CountAliveUnits(battle.blue_final)
            << " | red_missile_left=" << SumRemainingMissiles(battle.red_final)
            << " | blue_missile_left=" << SumRemainingMissiles(battle.blue_final)
            << " | red_objective=" << (battle.red_objective_complete ? "done" : "running")
            << " | blue_objective=" << (battle.blue_objective_complete ? "done" : "running")
            << " | sim_rounds=" << battle.elapsed_steps
            << " | reason=" << battle.termination_reason
            << " | replay=" << (write_replay ? "on" : "off")
            << " | elapsed_ms=" << task_elapsed_ms.count()
            << std::endl;
    }

    const auto batch_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - batch_started);
    std::cout
        << "[INFO] batch runner finished"
        << " | processed_tasks=" << count
        << " | elapsed_ms=" << batch_elapsed_ms.count()
        << std::endl;
    return 0;
}

