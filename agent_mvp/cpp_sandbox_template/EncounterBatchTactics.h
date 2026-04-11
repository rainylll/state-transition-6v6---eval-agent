#pragma once

#include <map>
#include <string>
#include <vector>

#include "../../Source/Control/flyPID.h"
#include "../../Source/Tools/ZHANSHU.h"
#include "../../Source/Tools/interface.h"

namespace EncounterBatchTactics {

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

struct TeamTelemetry {
    int target_id = -1;
    int planned_salvos = 0;
    int fired_total = 0;
    int primary_attacker_count = 0;
    int decoy_count = 0;
    bool objective_complete = false;
    ZhanShu::SurroundSide side = ZhanShu::SurroundSide::West;
};

double Clamp01(double v);
double DegToRad(double deg);
double RadToDeg(double rad);
double LongitudeDegreesForMeters(double meters, double latitude_deg);
double LatitudeDegreesForMeters(double meters);
double GeoDistanceMeters(double lon1, double lat1, double lon2, double lat2);
BattlefieldSquare BuildBattlefieldSquare();
const BattlefieldSquare& GetBattlefieldSquare();
GeoPoint ClampPointToBattlefield(double longitude, double latitude);

class TeamTacticController {
public:
    struct TacticProfile {
        double surround_arc_degrees = 180.0;
        double surround_radius = 0.6;
        double mid_radius_ratio = 1.2;
        double altitude = 5000.0;
        double first_fire_range = 60000.0;
        double round_range_interval = 10000.0;
        ZhanShu::SurroundSide side = ZhanShu::SurroundSide::West;
    };

    TeamTacticController(int team, int tactic_id, GeoPoint scene_anchor_seed, std::string team_name);

    void Initialize(const PlaneState_S* planes, int count);
    void RefreshIfNeeded(const PlaneState_S* planes, int count, int step, int task_number, int total_tasks);
    bool UpdateObjective(const PlaneState_S* planes, int count);
    void Apply(PlaneState_S* planes, int count, int step, double dt);

    TeamTelemetry GetTelemetry(const PlaneState_S* planes, int count) const;
    int target_id() const;
    bool objective_complete() const;
    ZhanShu::SurroundSide side() const;

private:
    enum class PlaneRole {
        None = 0,
        Lead,
        Wing,
        Decoy,
        Escort,
        Return,
        Evade,
    };

    struct ThreatSnapshot {
        int warning_count = 0;
        int inbound_missile_count = 0;
        int nearest_shooter_id = -1;
        double nearest_missile_distance_m = 1.0e12;
        double warning_origin_distance_m = 1.0e12;
        GeoPoint threat_origin;
        bool has_threat_origin = false;
        bool severe = false;
    };

    struct DodgeState {
        bool active = false;
        bool missile_driven = false;
        int remaining_steps = 0;
        int lateral_sign = 1;
        int activation_step = -1;
        bool prioritize_survival = false;
        GeoPoint anchor;
        double desired_altitude = 5000.0;
        double desired_speed = 320.0;
    };

    struct PlaneControlState {
        PlaneRole role = PlaneRole::None;
        int fired_rounds = 0;
        int cooldown_steps = 0;
        int warning_streak = 0;
        int last_warning_step = -1;
        int last_dodge_log_step = -1000000;
        ThreatSnapshot last_threat;
        DodgeState dodge;
        FlightController attack_controller;
        FlightController decoy_controller;
        FlightController evade_controller;
        FlightController return_controller;
    };

    struct TeamMissionPlan {
        GeoPoint selection_anchor;
        GeoPoint return_anchor;
        GeoPoint target_reference;
        TacticProfile profile;
        std::vector<int> team_plane_ids;
        std::vector<int> primary_attackers;
        std::vector<int> decoys;
        std::vector<int> escorts;
        int target_id = -1;
        int planned_salvos = 0;
        int initial_team_missiles = 0;
        int completion_hold_steps = 0;
        int last_refresh_step = 0;
        int geometry_refresh_count = 0;
        int sync_wait_steps = 0;
        int sync_wait_plane_id = -1;
        bool objective_complete = false;
        std::map<int, PlaneControlState> plane_states;
    };

    const PlaneState_S* FindPlaneById(const PlaneState_S* planes, int count, int plane_id) const;
    PlaneState_S* FindPlaneById(PlaneState_S* planes, int count, int plane_id) const;
    GeoPoint ComputeAnchorForPlaneIds(const PlaneState_S* planes, int count, const std::vector<int>& plane_ids, double default_lon, double default_lat) const;
    GeoPoint ComputeTeamAnchorFromPlanes(const PlaneState_S* planes, int count, double default_lon, double default_lat) const;
    int FindNearestEnemyToAnchor(const PlaneState_S* planes, int count, const GeoPoint& anchor) const;
    int CountAliveEnemies(const PlaneState_S* planes, int count) const;
    int SumTeamMissiles(const PlaneState_S* planes, int count) const;
    bool AreAliveTeamPlanesNearAnchor(const PlaneState_S* planes, int count, const GeoPoint& anchor, double max_distance_m) const;
    bool HasCapturedTarget(const PlaneState_S& plane, int target_id) const;
    ThreatSnapshot AnalyzeThreatForPlane(const PlaneState_S& self, const PlaneState_S* planes, int count) const;
    void RefreshPlan(const PlaneState_S* planes, int count, int step, bool force_log, int task_number, int total_tasks, const char* reason);
    void AssignRoles(const PlaneState_S* planes, int count);
    void ApplyReturnBehavior(PlaneState_S& plane, PlaneControlState& state, double dt) const;
    void ApplyDecoyBehavior(PlaneState_S& plane, PlaneControlState& state, const PlaneState_S& target, int decoy_index, int decoy_count, double dt);
    void ApplyAttackBehavior(PlaneState_S& plane, PlaneControlState& state, const PlaneState_S& target, int attacker_index, int attacker_count, double dt);
    void MaybeActivateDodge(const PlaneState_S& plane, PlaneControlState& state, const ThreatSnapshot& threat, int step, bool prioritize_survival);
    void ApplyDodgeBehavior(PlaneState_S& plane, PlaneControlState& state, const ThreatSnapshot& threat, int step, double dt);
    void HandleVolleyFire(PlaneState_S* planes, int count, const PlaneState_S& target, int step);
    bool CanPlaneFireNow(const PlaneState_S& plane, const PlaneControlState& state, int target_id, double fire_range_m) const;
    GeoPoint BuildAttackPoint(const PlaneState_S& target, int attacker_index, int attacker_count) const;
    GeoPoint BuildDecoyPoint(const PlaneState_S& target, int decoy_index, int decoy_count) const;
    std::string BuildTacviewLabel(const PlaneState_S& plane, const PlaneControlState& state) const;
    const char* RoleName(PlaneRole role) const;

    int team_ = 0;
    int tactic_id_ = 0;
    GeoPoint scene_anchor_seed_;
    std::string team_name_;
    TeamMissionPlan plan_;
};

} // namespace EncounterBatchTactics
