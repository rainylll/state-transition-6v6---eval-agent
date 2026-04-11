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

static double Clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

static double DegToRad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

static double RadToDeg(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
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

static BattleOutcome RunTactic(
    const std::vector<Unit7D>& red_units,
    const std::vector<Unit7D>& blue_units,
    int tactic_id,
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

    const double dt = 0.1;
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
        tactic_id,
        EncounterBatchTactics::GeoPoint{red_scene_anchor_seed.lon, red_scene_anchor_seed.lat},
        "red");
    EncounterBatchTactics::TeamTacticController blue_tactic(
        2,
        tactic_id,
        EncounterBatchTactics::GeoPoint{blue_scene_anchor_seed.lon, blue_scene_anchor_seed.lat},
        "blue");
    red_tactic.Initialize(planes.data(), plane_count);
    blue_tactic.Initialize(planes.data(), plane_count);
    bool should_sync_from_engine = false;
    bool reached_terminal_state = false;
    const auto initial_red_telemetry = red_tactic.GetTelemetry(planes.data(), plane_count);
    const auto initial_blue_telemetry = blue_tactic.GetTelemetry(planes.data(), plane_count);

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

    for (int step = 0; step < kMissionSafetyMaxSteps; ++step) {
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
        const bool red_objective_complete = red_tactic.UpdateObjective(planes.data(), plane_count);
        const bool blue_objective_complete = blue_tactic.UpdateObjective(planes.data(), plane_count);

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

        if (red_alive == 0 || blue_alive == 0) {
            out.termination_reason = (red_alive == 0) ? "red force destroyed" : "blue force destroyed";
            out.elapsed_steps = step + 1;
            reached_terminal_state = true;
            break;
        }
        if (red_objective_complete || blue_objective_complete) {
            if (red_objective_complete && blue_objective_complete) {
                out.termination_reason = "both objectives completed";
            } else if (red_objective_complete) {
                out.termination_reason = "red objective completed";
            } else {
                out.termination_reason = "blue objective completed";
            }
            out.elapsed_steps = step + 1;
            reached_terminal_state = true;
            break;
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
    }

    if (!reached_terminal_state) {
        out.termination_reason = "mission safety cap reached";
        out.elapsed_steps = kMissionSafetyMaxSteps;
        std::cout
            << "[TASK " << task_number << "/" << total_tasks << "] warning"
            << " | reason=" << out.termination_reason
            << " | max_rounds=" << kMissionSafetyMaxSteps
            << std::endl;
    }

    SyncFromEngine(planes.data(), plane_count);
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
    const int kMaxTasks = 1000;
    const int total_tasks = CountCandidateTasks(input_path, kMaxTasks);
    const BattlefieldSquare& battlefield = GetBattlefieldSquare();

    std::ifstream fin(input_path);
    std::ofstream fout(output_path, std::ios::out | std::ios::trunc);
    if (!fin.is_open() || !fout.is_open()) {
        std::cerr << "[ERROR] cannot open simulation_tasks.jsonl or episodes.jsonl" << std::endl;
        return 1;
    }

    const auto batch_started = std::chrono::steady_clock::now();
    std::cout
        << "[INFO] batch runner started"
        << " | input=" << input_path
        << " | output=" << output_path
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
        const int tactic_id = task.value("tactic_id", 0);
        const int task_number = count + 1;

        const auto red_units = ParseUnits7D(task["initial_state"]["red_features"]);
        const auto blue_units = ParseUnits7D(task["initial_state"]["blue_features"]);
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
            << " | tactic=" << tactic_id
            << " | replay=" << (write_replay ? "on" : "off")
            << std::endl;

        // Real tactic execution backed by engine InitEnv/EnvStep APIs.
        const BattleOutcome battle = RunTactic(
            red_units,
            blue_units,
            tactic_id,
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
        ++count;

        std::cout
            << "[TASK " << count << "/" << total_tasks << "] "
            << "completed"
            << " | task_id=" << task_id
            << " | tactic=" << tactic_id
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

