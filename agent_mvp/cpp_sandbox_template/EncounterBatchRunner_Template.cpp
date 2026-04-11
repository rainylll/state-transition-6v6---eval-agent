#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "EncounterBatchTactics_Template.h"
#include "../../Source/Tools/interface.h"

#define ENABLE_ACMI_REPLAY 1
#define ACMI_REPLAY_COUNT 10

static const char* kReplayDirectory = "agent_mvp/data_real/replays";

using json = nlohmann::json;

namespace {

// This file is the tactical-team integration template runner.
// It intentionally keeps the orchestration logic visible and heavily
// commented so tactical teams can focus on replacing the tactic module
// without touching the JSON contract or the engine stepping loop.

// ---------------------------------------------------------------------------
// Contract-facing data structures.
// The runner owns JSON parsing and JSON writing so tactical teams do not need
// to touch the simulation_tasks.jsonl / episodes.jsonl contract plumbing.
// ---------------------------------------------------------------------------
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
};

struct GeoPoint {
    double lon = 0.0;
    double lat = 0.0;
};

struct BattlefieldSquare {
    GeoPoint center;
    double safe_min_lon = 0.0;
    double safe_max_lon = 0.0;
    double safe_min_lat = 0.0;
    double safe_max_lat = 0.0;
    double side_length_m = 0.0;
};

constexpr double kRedBaseLongitude = 116.200000;
constexpr double kRedBaseLatitude = 21.577106;
constexpr double kBlueBaseLongitude = 119.800000;
constexpr double kBlueBaseLatitude = 21.577106;
constexpr double kBattlefieldSquareSideMeters = 600000.0;
constexpr double kBattlefieldEdgeMarginMeters = 60000.0;
constexpr int kMissionSafetyMaxSteps = 9000;
constexpr int kMissionLogIntervalSteps = 200;

double DegToRad(double deg)
{
    return deg * 3.14159265358979323846 / 180.0;
}

double RadToDeg(double rad)
{
    return rad * 180.0 / 3.14159265358979323846;
}

double Clamp01(double v)
{
    return std::max(0.0, std::min(1.0, v));
}

double LongitudeDegreesForMeters(double meters, double latitude_deg)
{
    const double cos_lat = std::max(0.2, std::cos(DegToRad(latitude_deg)));
    return meters / (111320.0 * cos_lat);
}

double LatitudeDegreesForMeters(double meters)
{
    return meters / 111000.0;
}

double GeoDistanceMeters(double lon1, double lat1, double lon2, double lat2)
{
    const double earth_radius_m = 6371000.0;
    const double dlat = DegToRad(lat2 - lat1);
    const double dlon = DegToRad(lon2 - lon1);
    const double a = std::sin(dlat * 0.5) * std::sin(dlat * 0.5) +
                     std::cos(DegToRad(lat1)) * std::cos(DegToRad(lat2)) *
                         std::sin(dlon * 0.5) * std::sin(dlon * 0.5);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return earth_radius_m * c;
}

BattlefieldSquare BuildBattlefieldSquare()
{
    BattlefieldSquare square;
    square.center = GeoPoint{
        (kRedBaseLongitude + kBlueBaseLongitude) * 0.5,
        (kRedBaseLatitude + kBlueBaseLatitude) * 0.5};
    square.side_length_m = kBattlefieldSquareSideMeters;

    const double half_lon_deg = LongitudeDegreesForMeters(square.side_length_m * 0.5, square.center.lat);
    const double half_lat_deg = LatitudeDegreesForMeters(square.side_length_m * 0.5);
    const double edge_lon_deg = LongitudeDegreesForMeters(kBattlefieldEdgeMarginMeters, square.center.lat);
    const double edge_lat_deg = LatitudeDegreesForMeters(kBattlefieldEdgeMarginMeters);

    square.safe_min_lon = square.center.lon - half_lon_deg + edge_lon_deg;
    square.safe_max_lon = square.center.lon + half_lon_deg - edge_lon_deg;
    square.safe_min_lat = square.center.lat - half_lat_deg + edge_lat_deg;
    square.safe_max_lat = square.center.lat + half_lat_deg - edge_lat_deg;
    return square;
}

const BattlefieldSquare& GetBattlefieldSquare()
{
    static const BattlefieldSquare square = BuildBattlefieldSquare();
    return square;
}

GeoPoint ClampPointToBattlefield(double lon, double lat)
{
    const BattlefieldSquare& square = GetBattlefieldSquare();
    return GeoPoint{
        std::clamp(lon, square.safe_min_lon, square.safe_max_lon),
        std::clamp(lat, square.safe_min_lat, square.safe_max_lat)};
}

double ComputeBearingDegrees(double lon1, double lat1, double lon2, double lat2)
{
    const double mean_lat = (lat1 + lat2) * 0.5;
    const double dx = (lon2 - lon1) * 111000.0 * std::cos(DegToRad(mean_lat));
    const double dy = (lat2 - lat1) * 111000.0;
    double bearing = RadToDeg(std::atan2(dx, dy));
    if (bearing < 0.0) {
        bearing += 360.0;
    }
    return bearing;
}

void ConfigurePlaneFrom7D(
    PlaneState_S& plane,
    const Unit7D& in,
    int team,
    int plane_id,
    double yaw_deg,
    double altitude,
    int limit_index)
{
    std::memset(&plane, 0, sizeof(PlaneState_S));
    plane.init(static_cast<char>(limit_index));

    const GeoPoint safe_start = ClampPointToBattlefield(in.lon, in.lat);
    plane._planeID = plane_id;
    plane._team = team;
    plane._targetID = -1;
    plane._equipmentType = FIGHTER_J15;
    plane._isAlive = in.alive > 0.5 ? 1 : 0;
    plane._longitude = safe_start.lon;
    plane._latitude = safe_start.lat;
    plane._altitude = altitude;
    plane._yaw = yaw_deg;
    plane._roll = 0.0;
    plane._pitch = 0.0;
    plane.TAS = std::max(0.0, in.speed);
    plane._throttle = 40.0;
    plane._missileCount = std::max(0, std::min(Max_Missile_Count, static_cast<int>(std::round(in.missile))));
    plane._radarState = RadarState_E::Scan;
    plane._radarMode = RadarMode_E::RM_Normal;
    plane._radar_hBeamWidth = 90.0;
    plane._radar_vBeamWidth = 60.0;
    plane._radar_all_hBeamWidth = 360.0;
    plane._radar_all_vBeamWidth = 180.0;
    if (in.sensor > 0.0) {
        const double radar_range_m = std::max(1000.0, in.sensor * 1000.0);
        plane._radar_range = radar_range_m;
        plane._radar_view_range = radar_range_m;
        plane._radar_all_range = radar_range_m;
    }
    std::snprintf(plane._pilot, sizeof(plane._pilot), "%d", plane_id);
}

void RestorePlaneContractState(PlaneState_S& plane, const Unit7D& in)
{
    const GeoPoint safe_start = ClampPointToBattlefield(in.lon, in.lat);
    plane._isAlive = in.alive > 0.5 ? 1 : 0;
    plane._longitude = safe_start.lon;
    plane._latitude = safe_start.lat;
    plane.TAS = std::max(0.0, in.speed);
    plane._throttle = 40.0;
    plane._targetID = -1;
    plane._isShoot = false;
    plane._missileCount = std::max(0, std::min(Max_Missile_Count, static_cast<int>(std::round(in.missile))));
    plane._radarState = RadarState_E::Scan;
    plane._radarMode = RadarMode_E::RM_Normal;
}

void SyncFromEngine(PlaneState_S* planes, int count)
{
    for (int i = 0; i < count; ++i) {
        const PlaneState_S* latest = GetPlaneState(planes[i]._planeID);
        if (latest != nullptr) {
            std::memcpy(&planes[i], latest, sizeof(PlaneState_S));
        }
    }
}

void ResetPlaneCommand(PlaneState_S& plane)
{
    plane._isShoot = false;
    plane._targetID = -1;
    plane._radarState = RadarState_E::Scan;
    plane._radarMode = RadarMode_E::RM_Normal;
    plane._isWaypointMode = false;
}

bool HasCapturedTarget(const PlaneState_S& plane, int target_id)
{
    const int capture_count = std::min(plane._raderCaptureCount, Max_Plane_Count);
    for (int i = 0; i < capture_count; ++i) {
        if (plane._raderCapturePlanesID[i] == target_id) {
            return true;
        }
    }
    return false;
}

int CountInboundMissiles(const PlaneState_S* planes, int count, int self_plane_id, int self_team)
{
    int inbound = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == self_team) {
            continue;
        }
        for (int m = 0; m < Max_Missile_Count; ++m) {
            const MissileState_S& missile = planes[i]._missileState[m];
            if (missile._targetID == self_plane_id && missile._missile_live == 1) {
                ++inbound;
            }
        }
    }
    return inbound;
}

EncounterBatchTemplate::UnitObservation BuildUnitObservation(
    const PlaneState_S& plane,
    const PlaneState_S* all_planes,
    int plane_count)
{
    EncounterBatchTemplate::UnitObservation obs;
    obs.plane_id = plane._planeID;
    obs.team = plane._team;
    obs.alive = plane._isAlive > 0;
    obs.missile_count = std::max(0, plane._missileCount);
    obs.warning_count = std::max(0, plane._recvRadarWarnCount);
    obs.inbound_missile_count = CountInboundMissiles(all_planes, plane_count, plane._planeID, plane._team);
    obs.target_locked = (plane._targetID >= 0) && HasCapturedTarget(plane, plane._targetID);
    obs.lon = plane._longitude;
    obs.lat = plane._latitude;
    obs.alt = plane._altitude;
    obs.speed = plane.TAS;
    return obs;
}

EncounterBatchTemplate::TeamStepInput BuildTeamStepInput(
    const std::string& task_id,
    int tactic_id,
    int team,
    int red_unit_count,
    int blue_unit_count,
    int step_index,
    double dt,
    const PlaneState_S* planes,
    int plane_count)
{
    EncounterBatchTemplate::TeamStepInput input;
    input.task.task_id = task_id;
    input.task.tactic_id = tactic_id;
    input.task.team = team;
    input.task.red_unit_count = red_unit_count;
    input.task.blue_unit_count = blue_unit_count;
    input.step_index = step_index;
    input.dt = dt;

    for (int i = 0; i < plane_count; ++i) {
        const auto obs = BuildUnitObservation(planes[i], planes, plane_count);
        if (planes[i]._team == team) {
            input.self_units.push_back(obs);
        } else {
            input.enemy_units.push_back(obs);
        }
    }
    return input;
}

PlaneState_S* FindPlaneById(PlaneState_S* planes, int count, int plane_id)
{
    for (int i = 0; i < count; ++i) {
        if (planes[i]._planeID == plane_id) {
            return &planes[i];
        }
    }
    return nullptr;
}

void ApplyTemplateCommand(PlaneState_S& plane, const EncounterBatchTemplate::UnitCommand& command)
{
    const GeoPoint desired = ClampPointToBattlefield(command.desired_lon, command.desired_lat);
    const double desired_bearing = ComputeBearingDegrees(
        plane._longitude,
        plane._latitude,
        desired.lon,
        desired.lat);

    plane._targetID = command.target_id;
    plane._radarState = command.radar_state;
    plane._radarMode = RadarMode_E::RM_Normal;
    plane._yaw = desired_bearing;
    plane._yaw_ctrl = command.evade ? 1.0 : 0.8;
    plane._roll_ctrl = command.evade ? 1.0 : 0.6;

    const double altitude_error = command.desired_alt - plane._altitude;
    plane._pitch = (altitude_error >= 0.0) ? 8.0 : -6.0;
    plane._pitch_ctrl = std::min(1.0, 0.2 + std::abs(altitude_error) / 4000.0);
    plane._throttle = (command.desired_speed > plane.TAS) ? 70.0 : 45.0;

    if (!command.debug_role.empty()) {
        std::snprintf(plane._pilot, sizeof(plane._pilot), "%s", command.debug_role.c_str());
    }

    if (command.fire && command.target_id >= 0 && HasCapturedTarget(plane, command.target_id)) {
        plane._isShoot = true;
        plane._radarState = RadarState_E::Track;
    }
}

void ApplyTemplateCommands(
    PlaneState_S* planes,
    int plane_count,
    const std::vector<EncounterBatchTemplate::UnitCommand>& commands)
{
    for (const auto& command : commands) {
        PlaneState_S* plane = FindPlaneById(planes, plane_count, command.plane_id);
        if (plane == nullptr || plane->_isAlive <= 0) {
            continue;
        }
        ApplyTemplateCommand(*plane, command);
    }
}

int CountAliveByTeam(const PlaneState_S* planes, int count, int team)
{
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == team && planes[i]._isAlive > 0) {
            ++alive;
        }
    }
    return alive;
}

int CountAliveUnits(const std::vector<Unit2D>& units)
{
    int alive = 0;
    for (const auto& unit : units) {
        if (unit.final_alive > 0.5) {
            ++alive;
        }
    }
    return alive;
}

int SumRemainingMissiles(const std::vector<Unit2D>& units)
{
    int missiles = 0;
    for (const auto& unit : units) {
        missiles += std::max(0, static_cast<int>(std::round(unit.final_missile)));
    }
    return missiles;
}

std::string BuildReplayPath(const std::string& task_id, int replay_index)
{
    std::filesystem::create_directories(kReplayDirectory);
    return std::string(kReplayDirectory) + "/template_" + std::to_string(replay_index + 1) + "_" + task_id + ".acmi";
}

int CountCandidateTasks(const std::string& input_path, int max_tasks)
{
    std::ifstream fin(input_path);
    if (!fin.is_open()) {
        return 0;
    }
    std::string line;
    int count = 0;
    while (count < max_tasks && std::getline(fin, line)) {
        if (!line.empty()) {
            ++count;
        }
    }
    return count;
}

std::vector<Unit7D> ParseUnits7D(const json& features)
{
    std::vector<Unit7D> out;
    if (!features.is_array()) {
        return out;
    }

    for (const auto& row : features) {
        if (!row.is_array() || row.size() < 7) {
            continue;
        }
        Unit7D unit;
        unit.type_id = row.at(0).get<double>();
        unit.speed = row.at(1).get<double>();
        unit.sensor = row.at(2).get<double>();
        unit.missile = row.at(3).get<double>();
        unit.lon = row.at(4).get<double>();
        unit.lat = row.at(5).get<double>();
        unit.alive = row.at(6).get<double>();
        out.push_back(unit);
    }
    return out;
}

struct RunnerOptions {
    std::string input_path = "simulation_tasks.jsonl";
    std::string output_path = "agent_mvp/data_real/raw/episodes_template.jsonl";
    int max_tasks = 1000;
};

enum class ParseResult {
    Ok,
    Help,
    Error,
};

void PrintTemplateRunnerUsage()
{
    std::cout
        << "Usage: EncounterBatchRunnerTemplate [--input-path <path>] [--out-path <path>] [--max-tasks <n>]" << std::endl
        << "Defaults:" << std::endl
        << "  --input-path simulation_tasks.jsonl" << std::endl
        << "  --out-path   agent_mvp/data_real/raw/episodes_template.jsonl" << std::endl
        << "  --max-tasks  1000" << std::endl;
}

ParseResult ParseRunnerOptions(int argc, char** argv, RunnerOptions& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintTemplateRunnerUsage();
            return ParseResult::Help;
        }

        if (arg == "--input-path") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] missing value for --input-path" << std::endl;
                return ParseResult::Error;
            }
            options.input_path = argv[++i];
            continue;
        }

        if (arg == "--out-path") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] missing value for --out-path" << std::endl;
                return ParseResult::Error;
            }
            options.output_path = argv[++i];
            continue;
        }

        if (arg == "--max-tasks") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] missing value for --max-tasks" << std::endl;
                return ParseResult::Error;
            }
            try {
                options.max_tasks = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "[ERROR] invalid integer for --max-tasks" << std::endl;
                return ParseResult::Error;
            }
            if (options.max_tasks <= 0) {
                std::cerr << "[ERROR] --max-tasks must be positive" << std::endl;
                return ParseResult::Error;
            }
            continue;
        }

        std::cerr << "[ERROR] unknown argument: " << arg << std::endl;
        PrintTemplateRunnerUsage();
        return ParseResult::Error;
    }

    return ParseResult::Ok;
}

BattleOutcome RunTemplateBattle(
    const std::vector<Unit7D>& red_units,
    const std::vector<Unit7D>& blue_units,
    int tactic_id,
    bool write_replay,
    int replay_index,
    const std::string& task_id,
    int task_number,
    int total_tasks)
{
    BattleOutcome outcome;
    outcome.red_final.resize(red_units.size());
    outcome.blue_final.resize(blue_units.size());

    const int red_count = static_cast<int>(red_units.size());
    const int blue_count = static_cast<int>(blue_units.size());
    const int plane_count = red_count + blue_count;
    std::vector<PlaneState_S> planes(plane_count);

    for (int i = 0; i < red_count; ++i) {
        ConfigurePlaneFrom7D(planes[i], red_units[i], 1, 10010 + i, 90.0, 9000.0, 0);
    }
    for (int i = 0; i < blue_count; ++i) {
        ConfigurePlaneFrom7D(planes[red_count + i], blue_units[i], 2, 20010 + i, 270.0, 9000.0, 0);
    }

    const std::string acmi_path = write_replay ? BuildReplayPath(task_id, replay_index) : std::string();
    const char* acmi_cstr = write_replay ? acmi_path.c_str() : nullptr;
    if (!InitEnv(acmi_cstr, planes.data(), static_cast<uint32_t>(plane_count))) {
        outcome.termination_reason = "InitEnv failed";
        return outcome;
    }

    for (int i = 0; i < red_count; ++i) {
        RestorePlaneContractState(planes[i], red_units[i]);
    }
    for (int i = 0; i < blue_count; ++i) {
        RestorePlaneContractState(planes[red_count + i], blue_units[i]);
    }

    EncounterBatchTemplate::TemplateTacticController red_tactic(1);
    EncounterBatchTemplate::TemplateTacticController blue_tactic(2);
    red_tactic.StartTask({task_id, tactic_id, 1, red_count, blue_count});
    blue_tactic.StartTask({task_id, tactic_id, 2, red_count, blue_count});

    const double dt = 0.1;
    bool reached_terminal_state = false;
    for (int step = 0; step < kMissionSafetyMaxSteps; ++step) {
        SyncFromEngine(planes.data(), plane_count);

        const int red_alive = CountAliveByTeam(planes.data(), plane_count, 1);
        const int blue_alive = CountAliveByTeam(planes.data(), plane_count, 2);

        if ((step % kMissionLogIntervalSteps) == 0) {
            std::cout
                << "[TEMPLATE TASK " << task_number << "/" << total_tasks << "][ROUND " << step << "]"
                << " red_alive=" << red_alive
                << " blue_alive=" << blue_alive
                << std::endl;
        }

        if (red_alive == 0 || blue_alive == 0) {
            outcome.elapsed_steps = step + 1;
            outcome.termination_reason = (red_alive == 0) ? "red force destroyed" : "blue force destroyed";
            reached_terminal_state = true;
            break;
        }

        for (int i = 0; i < plane_count; ++i) {
            if (planes[i]._isAlive > 0) {
                ResetPlaneCommand(planes[i]);
            }
        }

        // Runner-owned adaptation:
        // 1. Pull raw PlaneState_S from the engine.
        // 2. Convert it into TeamStepInput.
        // 3. Call the tactic module.
        // 4. Translate UnitCommand back into PlaneState_S control fields.
        const auto red_input = BuildTeamStepInput(task_id, tactic_id, 1, red_count, blue_count, step, dt, planes.data(), plane_count);
        const auto blue_input = BuildTeamStepInput(task_id, tactic_id, 2, red_count, blue_count, step, dt, planes.data(), plane_count);
        const auto red_commands = red_tactic.BuildStepCommands(red_input);
        const auto blue_commands = blue_tactic.BuildStepCommands(blue_input);

        ApplyTemplateCommands(planes.data(), plane_count, red_commands);
        ApplyTemplateCommands(planes.data(), plane_count, blue_commands);

        if (!EnvStep(dt, planes.data())) {
            outcome.elapsed_steps = step + 1;
            outcome.termination_reason = "EnvStep failed";
            reached_terminal_state = true;
            break;
        }
    }

    if (!reached_terminal_state) {
        outcome.elapsed_steps = kMissionSafetyMaxSteps;
        outcome.termination_reason = "mission safety cap reached";
    }

    SyncFromEngine(planes.data(), plane_count);
    red_tactic.FinishTask();
    blue_tactic.FinishTask();

    for (int i = 0; i < red_count; ++i) {
        outcome.red_final[i].final_missile = std::max(0.0, static_cast<double>(planes[i]._missileCount));
        outcome.red_final[i].final_alive = planes[i]._isAlive > 0 ? 1.0 : 0.0;
    }
    for (int i = 0; i < blue_count; ++i) {
        const int pidx = red_count + i;
        outcome.blue_final[i].final_missile = std::max(0.0, static_cast<double>(planes[pidx]._missileCount));
        outcome.blue_final[i].final_alive = planes[pidx]._isAlive > 0 ? 1.0 : 0.0;
    }

    const int red_alive_final = CountAliveUnits(outcome.red_final);
    const int blue_alive_final = CountAliveUnits(outcome.blue_final);
    const int red_missile_final = SumRemainingMissiles(outcome.red_final);
    const int blue_missile_final = SumRemainingMissiles(outcome.blue_final);

    if (red_alive_final != blue_alive_final) {
        outcome.red_win = red_alive_final > blue_alive_final ? 1 : 0;
    } else {
        outcome.red_win = red_missile_final >= blue_missile_final ? 1 : 0;
    }

    return outcome;
}

} // namespace

int main(int argc, char** argv)
{
    RunnerOptions options;
    const ParseResult parse_result = ParseRunnerOptions(argc, argv, options);
    if (parse_result == ParseResult::Help) {
        return 0;
    }
    if (parse_result == ParseResult::Error) {
        return 1;
    }

    const std::string& input_path = options.input_path;
    const std::string& output_path = options.output_path;
    const int kMaxTasks = options.max_tasks;
    const int total_tasks = CountCandidateTasks(input_path, kMaxTasks);

    const std::filesystem::path output_fs(output_path);
    if (output_fs.has_parent_path()) {
        std::filesystem::create_directories(output_fs.parent_path());
    }

    std::ifstream fin(input_path);
    std::ofstream fout(output_path, std::ios::out | std::ios::trunc);
    if (!fin.is_open() || !fout.is_open()) {
        std::cerr << "[ERROR] template runner cannot open input or output file" << std::endl;
        return 1;
    }

    std::cout
        << "[INFO] template runner started"
        << " | input=" << input_path
        << " | output=" << output_path
        << " | queued_tasks=" << total_tasks
        << " | replay_dir=" << kReplayDirectory
        << std::endl;

    std::string line;
    int processed_tasks = 0;
    while (processed_tasks < kMaxTasks && std::getline(fin, line)) {
        if (line.empty()) {
            continue;
        }

        json task;
        try {
            task = json::parse(line);
        } catch (const std::exception& e) {
            std::cerr << "[WARN] template runner skipped invalid json: " << e.what() << std::endl;
            continue;
        }

        const std::string task_id = task.value("task_id", "");
        const int tactic_id = task.value("tactic_id", 0);
        const int task_number = processed_tasks + 1;
        const auto red_units = ParseUnits7D(task["initial_state"]["red_features"]);
        const auto blue_units = ParseUnits7D(task["initial_state"]["blue_features"]);
        const bool write_replay = (ENABLE_ACMI_REPLAY == 1) && (processed_tasks < ACMI_REPLAY_COUNT);

        std::cout
            << "[TEMPLATE TASK " << task_number << "/" << total_tasks << "] starting"
            << " | task_id=" << task_id
            << " | tactic=" << tactic_id
            << " | replay=" << (write_replay ? "on" : "off")
            << std::endl;

        // The tactical team usually edits only:
        // - EncounterBatchTactics_Template.h
        // - EncounterBatchTactics_Template.cpp
        //
        // The runner continues to own:
        // - reading simulation_tasks.jsonl
        // - parsing 7D state
        // - InitEnv / EnvStep
        // - final 2D result collection
        // - writing episodes jsonl (default: episodes_template.jsonl)
        const BattleOutcome battle = RunTemplateBattle(
            red_units,
            blue_units,
            tactic_id,
            write_replay,
            processed_tasks,
            task_id,
            task_number,
            total_tasks);

        json ep;
        ep["task_id"] = task_id;
        ep["outcome"] = {{"red_win", battle.red_win}};
        ep["final_state"]["red_features"] = json::array();
        ep["final_state"]["blue_features"] = json::array();
        for (const auto& unit : battle.red_final) {
            ep["final_state"]["red_features"].push_back({unit.final_missile, unit.final_alive});
        }
        for (const auto& unit : battle.blue_final) {
            ep["final_state"]["blue_features"].push_back({unit.final_missile, unit.final_alive});
        }

        fout << ep.dump() << "\n";
        ++processed_tasks;

        std::cout
            << "[TEMPLATE TASK " << processed_tasks << "/" << total_tasks << "] completed"
            << " | task_id=" << task_id
            << " | red_win=" << battle.red_win
            << " | red_alive=" << CountAliveUnits(battle.red_final)
            << " | blue_alive=" << CountAliveUnits(battle.blue_final)
            << " | red_missile_left=" << SumRemainingMissiles(battle.red_final)
            << " | blue_missile_left=" << SumRemainingMissiles(battle.blue_final)
            << " | sim_rounds=" << battle.elapsed_steps
            << " | reason=" << battle.termination_reason
            << std::endl;
    }

    std::cout
        << "[INFO] template runner finished"
        << " | processed_tasks=" << processed_tasks
        << std::endl;
    return 0;
}
