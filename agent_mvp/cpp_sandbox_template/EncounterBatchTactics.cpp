#include "EncounterBatchTactics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>

namespace EncounterBatchTactics {

namespace {

constexpr double kRedBaseLongitude = 116.200000;
constexpr double kRedBaseLatitude = 21.577106;
constexpr double kBlueBaseLongitude = 119.800000;
constexpr double kBlueBaseLatitude = 21.577106;
constexpr double kBattlefieldSquareSideMeters = 600000.0;
constexpr double kBattlefieldEdgeMarginMeters = 60000.0;
constexpr double kReturnCompletionRadiusMeters = 15000.0;
constexpr int kMissionCompletionHoldSteps = 20;
constexpr double kFireReadyHoldTimeoutSteps = 45.0;
constexpr double kInvalidThreatDistance = 1.0e12;

double ClampTo(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

double GeoDistance3DMeters(const PlaneState_S& a, const MissileState_S& b) {
    const double horizontal = GeoDistanceMeters(
        a._longitude,
        a._latitude,
        b._longitude,
        b._latitude);
    const double vertical = a._altitude - b._altitude;
    return std::sqrt(horizontal * horizontal + vertical * vertical);
}

double GeoDistance3DMeters(const PlaneState_S& a, const PlaneState_S& b) {
    const double horizontal = GeoDistanceMeters(
        a._longitude,
        a._latitude,
        b._longitude,
        b._latitude);
    const double vertical = a._altitude - b._altitude;
    return std::sqrt(horizontal * horizontal + vertical * vertical);
}

bool HasUsableMissileTrack(const MissileState_S& missile) {
    return missile._missileID > 0 &&
           std::abs(missile._longitude) > 1.0 &&
           std::abs(missile._latitude) > 1.0;
}

GeoPoint GetBaseAnchorForTeam(int team) {
    if (team == 1) {
        return GeoPoint{kRedBaseLongitude, kRedBaseLatitude};
    }
    return GeoPoint{kBlueBaseLongitude, kBlueBaseLatitude};
}

ZhanShu::SurroundSide ChooseAdaptiveSurroundSide(const GeoPoint& selection_anchor, const GeoPoint& target_point) {
    return (selection_anchor.lon <= target_point.lon) ? ZhanShu::SurroundSide::West : ZhanShu::SurroundSide::East;
}

double ComputeAverageSensorRangeMeters(
    const PlaneState_S* planes,
    int count,
    const std::vector<int>& plane_ids,
    double default_value_m) {
    double range_sum = 0.0;
    int samples = 0;
    for (int plane_id : plane_ids) {
        for (int i = 0; i < count; ++i) {
            if (planes[i]._planeID != plane_id || planes[i]._isAlive <= 0) {
                continue;
            }
            const double sensor_range = (planes[i]._radar_all_range > 0.0) ? planes[i]._radar_all_range : planes[i]._radar_range;
            if (sensor_range > 0.0) {
                range_sum += sensor_range;
                ++samples;
            }
            break;
        }
    }

    if (samples == 0) {
        return default_value_m;
    }
    return range_sum / static_cast<double>(samples);
}

GeoPoint BuildAdaptiveReturnAnchor(
    const GeoPoint& selection_anchor,
    const GeoPoint& target_point,
    const GeoPoint& fallback_anchor) {
    const double mean_lat = (selection_anchor.lat + target_point.lat) * 0.5;
    const double dx_m = (selection_anchor.lon - target_point.lon) * 111000.0 * std::cos(DegToRad(mean_lat));
    const double dy_m = (selection_anchor.lat - target_point.lat) * 111000.0;
    const double norm_m = std::sqrt(dx_m * dx_m + dy_m * dy_m);
    if (norm_m < 1000.0) {
        return ClampPointToBattlefield(fallback_anchor.lon, fallback_anchor.lat);
    }

    const double retreat_m = ClampTo(norm_m * 0.18, 15000.0, 45000.0);
    const double ux = dx_m / norm_m;
    const double uy = dy_m / norm_m;
    return ClampPointToBattlefield(
        selection_anchor.lon + LongitudeDegreesForMeters(ux * retreat_m, selection_anchor.lat),
        selection_anchor.lat + LatitudeDegreesForMeters(uy * retreat_m));
}

TeamTacticController::TacticProfile BuildAdaptiveTacticProfile(
    int tactic_id,
    const PlaneState_S* planes,
    int count,
    const std::vector<int>& attack_plane_ids,
    const GeoPoint& selection_anchor,
    const GeoPoint& target_point,
    double target_altitude) {
    const double distance_m = GeoDistanceMeters(
        selection_anchor.lon,
        selection_anchor.lat,
        target_point.lon,
        target_point.lat);
    const double distance_deg = distance_m / 111000.0;
    const double avg_sensor_m = ComputeAverageSensorRangeMeters(
        planes,
        count,
        attack_plane_ids,
        ((tactic_id % 2) == 0) ? 120000.0 : 80000.0);

    TeamTacticController::TacticProfile profile;
    profile.side = ChooseAdaptiveSurroundSide(selection_anchor, target_point);

    if ((tactic_id % 2) == 0) {
        profile.surround_arc_degrees = ClampTo(45.0 + distance_m / 2500.0, 50.0, 95.0);
        profile.surround_radius = ClampTo(distance_deg * 0.35, 0.20, 0.75);
        profile.mid_radius_ratio = ClampTo(1.08 + distance_deg * 0.08, 1.08, 1.25);
        profile.altitude = ClampTo(target_altitude + 2000.0, 8000.0, 12000.0);
        profile.first_fire_range = ClampTo(std::min(avg_sensor_m * 0.90, distance_m * 0.90), 45000.0, 120000.0);
        profile.round_range_interval = ClampTo(profile.first_fire_range * 0.12, 10000.0, 16000.0);
    } else {
        profile.surround_arc_degrees = ClampTo(120.0 + distance_m / 2500.0, 120.0, 180.0);
        profile.surround_radius = ClampTo(distance_deg * 0.28, 0.12, 0.55);
        profile.mid_radius_ratio = ClampTo(1.10 + distance_deg * 0.08, 1.10, 1.22);
        profile.altitude = ClampTo(std::max(4500.0, target_altitude), 4500.0, 8000.0);
        profile.first_fire_range = ClampTo(std::min(avg_sensor_m * 0.75, distance_m * 0.85), 30000.0, 80000.0);
        profile.round_range_interval = ClampTo(profile.first_fire_range * 0.15, 8000.0, 12000.0);
    }

    if (attack_plane_ids.size() <= 1) {
        profile.surround_arc_degrees = std::min(profile.surround_arc_degrees, 80.0);
        profile.surround_radius *= 0.75;
        profile.mid_radius_ratio = std::min(profile.mid_radius_ratio, 1.12);
    }

    return profile;
}

} // namespace

double Clamp01(double v) {
    return std::max(0.0, std::min(1.0, v));
}

double DegToRad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

double RadToDeg(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
}

double LongitudeDegreesForMeters(double meters, double latitude_deg) {
    const double cos_lat = std::max(0.2, std::cos(DegToRad(latitude_deg)));
    return meters / (111320.0 * cos_lat);
}

double LatitudeDegreesForMeters(double meters) {
    return meters / 111000.0;
}

double GeoDistanceMeters(double lon1, double lat1, double lon2, double lat2) {
    const double earth_radius_m = 6371000.0;
    const double dlat = DegToRad(lat2 - lat1);
    const double dlon = DegToRad(lon2 - lon1);
    const double a = std::sin(dlat * 0.5) * std::sin(dlat * 0.5) +
                     std::cos(DegToRad(lat1)) * std::cos(DegToRad(lat2)) *
                         std::sin(dlon * 0.5) * std::sin(dlon * 0.5);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return earth_radius_m * c;
}

BattlefieldSquare BuildBattlefieldSquare() {
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

const BattlefieldSquare& GetBattlefieldSquare() {
    static const BattlefieldSquare square = BuildBattlefieldSquare();
    return square;
}

GeoPoint ClampPointToBattlefield(double longitude, double latitude) {
    const BattlefieldSquare& square = GetBattlefieldSquare();
    return GeoPoint{
        std::clamp(longitude, square.safe_min_lon, square.safe_max_lon),
        std::clamp(latitude, square.safe_min_lat, square.safe_max_lat)};
}

TeamTacticController::TeamTacticController(int team, int tactic_id, GeoPoint scene_anchor_seed, std::string team_name)
    : team_(team),
      tactic_id_(tactic_id),
      scene_anchor_seed_(scene_anchor_seed),
      team_name_(std::move(team_name)) {}

const PlaneState_S* TeamTacticController::FindPlaneById(const PlaneState_S* planes, int count, int plane_id) const {
    for (int i = 0; i < count; ++i) {
        if (planes[i]._planeID == plane_id) {
            return &planes[i];
        }
    }
    return nullptr;
}

PlaneState_S* TeamTacticController::FindPlaneById(PlaneState_S* planes, int count, int plane_id) const {
    for (int i = 0; i < count; ++i) {
        if (planes[i]._planeID == plane_id) {
            return &planes[i];
        }
    }
    return nullptr;
}

GeoPoint TeamTacticController::ComputeAnchorForPlaneIds(
    const PlaneState_S* planes,
    int count,
    const std::vector<int>& plane_ids,
    double default_lon,
    double default_lat) const {
    double lon_sum = 0.0;
    double lat_sum = 0.0;
    int alive = 0;
    for (int plane_id : plane_ids) {
        const PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
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

GeoPoint TeamTacticController::ComputeTeamAnchorFromPlanes(
    const PlaneState_S* planes,
    int count,
    double default_lon,
    double default_lat) const {
    double lon_sum = 0.0;
    double lat_sum = 0.0;
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team != team_ || planes[i]._isAlive <= 0) {
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

int TeamTacticController::FindNearestEnemyToAnchor(const PlaneState_S* planes, int count, const GeoPoint& anchor) const {
    int best_plane_id = -1;
    double best_dist = std::numeric_limits<double>::max();
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == team_ || planes[i]._isAlive <= 0) {
            continue;
        }
        const double d = GeoDistanceMeters(anchor.lon, anchor.lat, planes[i]._longitude, planes[i]._latitude);
        if (d < best_dist) {
            best_dist = d;
            best_plane_id = planes[i]._planeID;
        }
    }
    return best_plane_id;
}

int TeamTacticController::CountAliveEnemies(const PlaneState_S* planes, int count) const {
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team != team_ && planes[i]._isAlive > 0) {
            ++alive;
        }
    }
    return alive;
}

int TeamTacticController::SumTeamMissiles(const PlaneState_S* planes, int count) const {
    int missiles = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == team_) {
            missiles += std::max(0, planes[i]._missileCount);
        }
    }
    return missiles;
}

bool TeamTacticController::AreAliveTeamPlanesNearAnchor(
    const PlaneState_S* planes,
    int count,
    const GeoPoint& anchor,
    double max_distance_m) const {
    int alive = 0;
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team != team_ || planes[i]._isAlive <= 0) {
            continue;
        }
        ++alive;
        const double distance = GeoDistanceMeters(
            planes[i]._longitude,
            planes[i]._latitude,
            anchor.lon,
            anchor.lat);
        if (distance > max_distance_m) {
            return false;
        }
    }
    return alive > 0;
}

bool TeamTacticController::HasCapturedTarget(const PlaneState_S& plane, int target_id) const {
    const int capture_count = std::min(plane._raderCaptureCount, Max_Plane_Count);
    for (int i = 0; i < capture_count; ++i) {
        if (plane._raderCapturePlanesID[i] == target_id) {
            return true;
        }
    }
    return false;
}

TeamTacticController::ThreatSnapshot TeamTacticController::AnalyzeThreatForPlane(
    const PlaneState_S& self,
    const PlaneState_S* planes,
    int count) const {
    ThreatSnapshot threat;

    double warn_lon_sum = 0.0;
    double warn_lat_sum = 0.0;
    int warn_samples = 0;
    for (int i = 0; i < Max_Warn_Count; ++i) {
        const WarnState_S& warn = self._recvRadarWarnState[i];
        if (warn._targetID <= 0) {
            continue;
        }
        ++threat.warning_count;
        warn_lon_sum += warn._tarLongitude;
        warn_lat_sum += warn._tarLatitude;
        ++warn_samples;
    }

    if (warn_samples > 0) {
        threat.threat_origin = GeoPoint{
            warn_lon_sum / static_cast<double>(warn_samples),
            warn_lat_sum / static_cast<double>(warn_samples)};
        threat.has_threat_origin = true;
        threat.warning_origin_distance_m = GeoDistanceMeters(
            self._longitude,
            self._latitude,
            threat.threat_origin.lon,
            threat.threat_origin.lat);
    }

    for (int i = 0; i < count; ++i) {
        if (planes[i]._team == team_ || planes[i]._planeID == self._planeID) {
            continue;
        }
        const double shooter_distance = GeoDistance3DMeters(self, planes[i]);
        for (int m = 0; m < Max_Missile_Count; ++m) {
            const MissileState_S& missile = planes[i]._missileState[m];
            if (missile._missile_live != 1 || missile._targetID != self._planeID || missile._missileID <= 0) {
                continue;
            }

            ++threat.inbound_missile_count;
            double distance = kInvalidThreatDistance;
            GeoPoint threat_origin{planes[i]._longitude, planes[i]._latitude};
            bool has_origin = true;
            if (HasUsableMissileTrack(missile)) {
                distance = GeoDistance3DMeters(self, missile);
                threat_origin = GeoPoint{missile._longitude, missile._latitude};
            } else if (missile._distanceAtLaunch > 1.0) {
                distance = missile._distanceAtLaunch;
            } else {
                distance = shooter_distance;
            }

            if (distance < threat.nearest_missile_distance_m) {
                threat.nearest_missile_distance_m = distance;
                threat.nearest_shooter_id = planes[i]._planeID;
                if (has_origin) {
                    threat.threat_origin = threat_origin;
                    threat.has_threat_origin = true;
                }
            }
        }
    }

    if (threat.inbound_missile_count > 0 && threat.nearest_shooter_id == -1) {
        for (int i = 0; i < count; ++i) {
            if (planes[i]._team != team_) {
                threat.nearest_shooter_id = planes[i]._planeID;
                break;
            }
        }
    }

    threat.severe =
        (threat.inbound_missile_count > 0 && threat.nearest_missile_distance_m < 35000.0) ||
        threat.inbound_missile_count > 1 ||
        (threat.warning_count > 1 && threat.warning_origin_distance_m < 30000.0);
    return threat;
}

void TeamTacticController::AssignRoles(const PlaneState_S* planes, int count) {
    plan_.team_plane_ids.clear();
    std::vector<std::pair<double, int>> armed_by_distance;
    std::vector<std::pair<double, int>> unarmed_by_distance;

    const PlaneState_S* target = FindPlaneById(planes, count, plan_.target_id);
    for (int i = 0; i < count; ++i) {
        if (planes[i]._team != team_ || planes[i]._isAlive <= 0) {
            continue;
        }
        plan_.team_plane_ids.push_back(planes[i]._planeID);
        const double distance = (target != nullptr)
            ? GeoDistanceMeters(planes[i]._longitude, planes[i]._latitude, target->_longitude, target->_latitude)
            : 0.0;
        if (planes[i]._missileCount > 0) {
            armed_by_distance.push_back({distance, planes[i]._planeID});
        } else {
            unarmed_by_distance.push_back({distance, planes[i]._planeID});
        }
    }

    std::sort(armed_by_distance.begin(), armed_by_distance.end());
    std::sort(unarmed_by_distance.begin(), unarmed_by_distance.end());

    plan_.primary_attackers.clear();
    plan_.decoys.clear();
    plan_.escorts.clear();
    for (size_t i = 0; i < armed_by_distance.size(); ++i) {
        if (i < 2) {
            plan_.primary_attackers.push_back(armed_by_distance[i].second);
        } else {
            plan_.escorts.push_back(armed_by_distance[i].second);
        }
    }

    if (plan_.primary_attackers.size() == 1 && !unarmed_by_distance.empty()) {
        plan_.decoys.push_back(unarmed_by_distance.front().second);
        for (size_t i = 1; i < unarmed_by_distance.size(); ++i) {
            plan_.escorts.push_back(unarmed_by_distance[i].second);
        }
    } else {
        for (const auto& entry : unarmed_by_distance) {
            plan_.escorts.push_back(entry.second);
        }
    }

    plan_.planned_salvos = 0;
    if (plan_.primary_attackers.size() >= 2) {
        plan_.planned_salvos = Max_Missile_Count;
        for (int plane_id : plan_.primary_attackers) {
            const PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
            if (plane != nullptr) {
                plan_.planned_salvos = std::min(plan_.planned_salvos, std::max(0, plane->_missileCount));
            }
        }
    } else if (!plan_.primary_attackers.empty()) {
        const PlaneState_S* plane = FindPlaneById(planes, count, plan_.primary_attackers.front());
        plan_.planned_salvos = (plane != nullptr) ? std::max(0, plane->_missileCount) : 0;
    }
}

void TeamTacticController::RefreshPlan(
    const PlaneState_S* planes,
    int count,
    int step,
    bool force_log,
    int task_number,
    int total_tasks,
    const char* reason) {
    GeoPoint team_anchor = ComputeTeamAnchorFromPlanes(
        planes,
        count,
        scene_anchor_seed_.lon,
        scene_anchor_seed_.lat);

    const PlaneState_S* current_target = FindPlaneById(planes, count, plan_.target_id);
    if (plan_.target_id == -1 || current_target == nullptr || current_target->_isAlive <= 0) {
        plan_.target_id = FindNearestEnemyToAnchor(planes, count, team_anchor);
    }

    const PlaneState_S* target = FindPlaneById(planes, count, plan_.target_id);
    if (target == nullptr) {
        plan_.team_plane_ids.clear();
        plan_.primary_attackers.clear();
        plan_.decoys.clear();
        plan_.escorts.clear();
        return;
    }

    AssignRoles(planes, count);
    const std::vector<int>& anchor_planes = !plan_.primary_attackers.empty() ? plan_.primary_attackers : plan_.team_plane_ids;
    plan_.selection_anchor = ComputeAnchorForPlaneIds(
        planes,
        count,
        anchor_planes,
        team_anchor.lon,
        team_anchor.lat);
    plan_.target_reference = GeoPoint{target->_longitude, target->_latitude};
    plan_.return_anchor = BuildAdaptiveReturnAnchor(plan_.selection_anchor, plan_.target_reference, scene_anchor_seed_);
    plan_.profile = BuildAdaptiveTacticProfile(
        tactic_id_,
        planes,
        count,
        anchor_planes,
        plan_.selection_anchor,
        plan_.target_reference,
        target->_altitude);
    plan_.last_refresh_step = step;
    ++plan_.geometry_refresh_count;

    if (force_log) {
        std::cout
            << "[TASK " << task_number << "/" << total_tasks << "] adapt"
            << " | team=" << team_name_
            << " | reason=" << reason
            << " | target=" << plan_.target_id
            << " | attackers=" << plan_.primary_attackers.size()
            << " | decoys=" << plan_.decoys.size()
            << " | side=" << ((plan_.profile.side == ZhanShu::SurroundSide::West) ? "west" : "east")
            << " | radius_deg=" << plan_.profile.surround_radius
            << " | fire_range_m=" << plan_.profile.first_fire_range
            << " | return_lon=" << plan_.return_anchor.lon
            << " | return_lat=" << plan_.return_anchor.lat
            << std::endl;
    }
}

void TeamTacticController::Initialize(const PlaneState_S* planes, int count) {
    plan_ = TeamMissionPlan{};
    plan_.initial_team_missiles = SumTeamMissiles(planes, count);
    RefreshPlan(planes, count, 0, false, 0, 0, "init");
}

void TeamTacticController::RefreshIfNeeded(
    const PlaneState_S* planes,
    int count,
    int step,
    int task_number,
    int total_tasks) {
    const int current_team_missiles = SumTeamMissiles(planes, count);
    const int fired_total = std::max(0, plan_.initial_team_missiles - current_team_missiles);
    const PlaneState_S* target = FindPlaneById(planes, count, plan_.target_id);

    if (target == nullptr || target->_isAlive <= 0) {
        if (CountAliveEnemies(planes, count) > 0) {
            plan_.target_id = FindNearestEnemyToAnchor(planes, count, plan_.selection_anchor);
            RefreshPlan(planes, count, step, true, task_number, total_tasks, "retarget");
        }
        return;
    }

    if (fired_total > 0 || step - plan_.last_refresh_step < 120) {
        return;
    }

    const GeoPoint current_attack_anchor = ComputeAnchorForPlaneIds(
        planes,
        count,
        !plan_.primary_attackers.empty() ? plan_.primary_attackers : plan_.team_plane_ids,
        plan_.selection_anchor.lon,
        plan_.selection_anchor.lat);
    const double target_drift_m = GeoDistanceMeters(
        plan_.target_reference.lon,
        plan_.target_reference.lat,
        target->_longitude,
        target->_latitude);
    const double attacker_drift_m = GeoDistanceMeters(
        plan_.selection_anchor.lon,
        plan_.selection_anchor.lat,
        current_attack_anchor.lon,
        current_attack_anchor.lat);
    const double geometry_threshold_m = std::max(18000.0, plan_.profile.first_fire_range * 0.25);
    if (target_drift_m >= geometry_threshold_m || attacker_drift_m >= 12000.0) {
        RefreshPlan(planes, count, step, true, task_number, total_tasks, "geometry");
    }
}

bool TeamTacticController::UpdateObjective(const PlaneState_S* planes, int count) {
    if (plan_.objective_complete) {
        return true;
    }

    if (CountAliveEnemies(planes, count) == 0) {
        plan_.objective_complete = true;
        return true;
    }

    const int current_team_missiles = SumTeamMissiles(planes, count);
    const bool team_returned_home = AreAliveTeamPlanesNearAnchor(
        planes,
        count,
        plan_.return_anchor,
        kReturnCompletionRadiusMeters);

    if (current_team_missiles == 0 && team_returned_home) {
        ++plan_.completion_hold_steps;
    } else {
        plan_.completion_hold_steps = 0;
    }

    if (plan_.completion_hold_steps >= kMissionCompletionHoldSteps) {
        plan_.objective_complete = true;
    }
    return plan_.objective_complete;
}

GeoPoint TeamTacticController::BuildAttackPoint(
    const PlaneState_S& target,
    int attacker_index,
    int attacker_count) const {
    const double arc_center_deg = (plan_.profile.side == ZhanShu::SurroundSide::East) ? 0.0 : 180.0;
    const double half_spread_deg = std::min(plan_.profile.surround_arc_degrees * 0.5, 40.0);
    double angle_deg = arc_center_deg;
    if (attacker_count > 1) {
        const double t = static_cast<double>(attacker_index) / static_cast<double>(std::max(1, attacker_count - 1));
        angle_deg = (arc_center_deg - half_spread_deg) + t * (half_spread_deg * 2.0);
    }

    const double angle_rad = DegToRad(angle_deg);
    return ClampPointToBattlefield(
        target._longitude + plan_.profile.surround_radius * std::cos(angle_rad),
        target._latitude + plan_.profile.surround_radius * std::sin(angle_rad));
}

GeoPoint TeamTacticController::BuildDecoyPoint(
    const PlaneState_S& target,
    int decoy_index,
    int decoy_count) const {
    const double toward_target_m = 18000.0 + decoy_index * 4000.0;
    const double lateral_m = 16000.0 + decoy_count * 2000.0;

    const double mean_lat = (plan_.selection_anchor.lat + target._latitude) * 0.5;
    const double dx_m = (target._longitude - plan_.selection_anchor.lon) * 111000.0 * std::cos(DegToRad(mean_lat));
    const double dy_m = (target._latitude - plan_.selection_anchor.lat) * 111000.0;
    const double norm = std::sqrt(dx_m * dx_m + dy_m * dy_m);
    const double ux = (norm > 1.0) ? (dx_m / norm) : 1.0;
    const double uy = (norm > 1.0) ? (dy_m / norm) : 0.0;
    const double lx = -uy;
    const double ly = ux;
    const double lateral_sign = ((team_ == 1) ? 1.0 : -1.0) * ((decoy_index % 2 == 0) ? 1.0 : -1.0);

    return ClampPointToBattlefield(
        plan_.selection_anchor.lon + LongitudeDegreesForMeters(ux * toward_target_m + lx * lateral_m * lateral_sign, plan_.selection_anchor.lat),
        plan_.selection_anchor.lat + LatitudeDegreesForMeters(uy * toward_target_m + ly * lateral_m * lateral_sign));
}

void TeamTacticController::ApplyReturnBehavior(PlaneState_S& plane, PlaneControlState& state, double dt) const {
    state.return_controller.run_control(
        plane,
        plan_.return_anchor.lon,
        plan_.return_anchor.lat,
        plan_.profile.altitude,
        400.0,
        dt);
}

void TeamTacticController::MaybeActivateDodge(
    const PlaneState_S& plane,
    PlaneControlState& state,
    const ThreatSnapshot& threat,
    int step,
    bool prioritize_survival) {
    if (threat.warning_count > 0) {
        if (state.last_warning_step == step - 1) {
            ++state.warning_streak;
        } else {
            state.warning_streak = 1;
        }
        state.last_warning_step = step;
    } else {
        state.warning_streak = 0;
    }

    const bool missile_trigger =
        threat.inbound_missile_count > 0 &&
        threat.nearest_missile_distance_m < kInvalidThreatDistance;
    const bool radar_trigger = prioritize_survival
        ? (threat.warning_count > 0 &&
           state.warning_streak >= 30 &&
           threat.warning_origin_distance_m <= 35000.0)
        : (threat.warning_count > 1 &&
           state.warning_streak >= 40 &&
           threat.warning_origin_distance_m <= 26000.0);
    if (!missile_trigger && !radar_trigger) {
        return;
    }

    const bool was_active = state.dodge.active;
    const bool was_missile_driven = state.dodge.missile_driven;
    const bool stronger_missile_threat =
        missile_trigger &&
        (!was_missile_driven ||
         threat.nearest_missile_distance_m + 5000.0 < state.last_threat.nearest_missile_distance_m);
    const bool should_revector =
        !was_active ||
        stronger_missile_threat ||
        state.dodge.remaining_steps <= 12;
    if (!should_revector) {
        return;
    }

    const double threat_bearing = threat.has_threat_origin
        ? ZhanShu::BearingDeg(
            plane._latitude,
            plane._longitude,
            threat.threat_origin.lat,
            threat.threat_origin.lon)
        : plane._yaw;
    const double evade_heading = std::fmod(threat_bearing + 90.0 * static_cast<double>(state.dodge.lateral_sign) + 360.0, 360.0);
    const double forward_m = missile_trigger ? -8000.0 : -4000.0;
    const double lateral_m = missile_trigger ? 26000.0 : 18000.0;
    const double altitude_delta = missile_trigger ? 1200.0 : 600.0;

    state.dodge.active = true;
    state.dodge.missile_driven = missile_trigger;
    state.dodge.activation_step = step;
    state.dodge.prioritize_survival = prioritize_survival;
    state.dodge.remaining_steps = missile_trigger
        ? ((threat.nearest_missile_distance_m < 18000.0) ? 90 : 60)
        : 35;
    state.dodge.desired_speed = prioritize_survival ? 260.0 : (missile_trigger ? 300.0 : 340.0);
    state.dodge.desired_altitude = ClampTo(
        plane._altitude + ((state.dodge.lateral_sign > 0) ? altitude_delta : -altitude_delta),
        3500.0,
        11000.0);
    state.dodge.anchor = ClampPointToBattlefield(
        plane._longitude +
            LongitudeDegreesForMeters(
                std::cos(DegToRad(evade_heading)) * lateral_m + std::cos(DegToRad(threat_bearing)) * forward_m,
                plane._latitude),
        plane._latitude +
            LatitudeDegreesForMeters(
                std::sin(DegToRad(evade_heading)) * lateral_m + std::sin(DegToRad(threat_bearing)) * forward_m));
    state.dodge.lateral_sign *= -1;

    if (!was_active || stronger_missile_threat || step - state.last_dodge_log_step >= 30) {
        state.last_dodge_log_step = step;
        std::cout
            << "[TACTIC] team=" << team_name_
            << " | plane=" << plane._planeID
            << " | role=" << RoleName(state.role)
            << " | evade=" << (missile_trigger ? "missile" : "warn")
            << " | warn=" << threat.warning_count
            << " | inbound=" << threat.inbound_missile_count
            << " | nearest_m=" << ((threat.nearest_missile_distance_m > 9.0e11) ? -1.0 : threat.nearest_missile_distance_m)
            << std::endl;
    }
}

void TeamTacticController::ApplyDodgeBehavior(
    PlaneState_S& plane,
    PlaneControlState& state,
    const ThreatSnapshot& threat,
    int step,
    double dt) {
    state.role = PlaneRole::Evade;
    if (threat.inbound_missile_count > 0) {
        state.dodge.remaining_steps = std::max(state.dodge.remaining_steps, 30);
    }
    if (state.dodge.remaining_steps > 0) {
        --state.dodge.remaining_steps;
    }
    state.evade_controller.run_control(
        plane,
        state.dodge.anchor.lon,
        state.dodge.anchor.lat,
        state.dodge.desired_altitude,
        state.dodge.desired_speed,
        dt);
    plane._throttle = std::min(plane._throttle, state.dodge.desired_speed <= 280.0 ? 32.0 : 55.0);

    if (state.dodge.remaining_steps <= 0 && threat.warning_count == 0 && threat.inbound_missile_count == 0) {
        state.dodge.active = false;
    } else if (state.dodge.remaining_steps <= 10 && (threat.has_threat_origin || threat.inbound_missile_count > 0)) {
        MaybeActivateDodge(plane, state, threat, step, state.dodge.prioritize_survival);
    }
}

void TeamTacticController::ApplyDecoyBehavior(
    PlaneState_S& plane,
    PlaneControlState& state,
    const PlaneState_S& target,
    int decoy_index,
    int decoy_count,
    double dt) {
    const GeoPoint decoy_point = BuildDecoyPoint(target, decoy_index, decoy_count);
    state.decoy_controller.run_control(
        plane,
        decoy_point.lon,
        decoy_point.lat,
        std::max(4200.0, plan_.profile.altitude - 700.0),
        (state.last_threat.warning_count > 0) ? 330.0 : 390.0,
        dt);
    if (state.last_threat.warning_count > 0) {
        plane._throttle = std::min(plane._throttle, 46.0);
    }
}

void TeamTacticController::ApplyAttackBehavior(
    PlaneState_S& plane,
    PlaneControlState& state,
    const PlaneState_S& target,
    int attacker_index,
    int attacker_count,
    double dt) {
    const GeoPoint attack_point = BuildAttackPoint(target, attacker_index, attacker_count);
    const double dist_to_attack_point = GeoDistanceMeters(
        plane._longitude,
        plane._latitude,
        attack_point.lon,
        attack_point.lat);
    const double dist_to_target = GeoDistanceMeters(
        plane._longitude,
        plane._latitude,
        target._longitude,
        target._latitude);

    if (state.cooldown_steps > 0) {
        --state.cooldown_steps;
    }

    if (dist_to_attack_point > 12000.0 && !HasCapturedTarget(plane, plan_.target_id) &&
        dist_to_target > plan_.profile.first_fire_range * 0.75) {
        state.attack_controller.run_control(
            plane,
            attack_point.lon,
            attack_point.lat,
            plan_.profile.altitude,
            450.0,
            dt);
    } else {
        state.attack_controller.run_control(
            plane,
            target._longitude,
            target._latitude,
            plan_.profile.altitude,
            485.0,
            dt);
    }

    if (HasCapturedTarget(plane, plan_.target_id)) {
        plane._radarState = RadarState_E::Track;
        plane._targetID = plan_.target_id;
    }
}

bool TeamTacticController::CanPlaneFireNow(
    const PlaneState_S& plane,
    const PlaneControlState& state,
    int target_id,
    double fire_range_m) const {
    return plane._isAlive > 0 &&
           plane._missileCount > 0 &&
           state.cooldown_steps == 0 &&
           HasCapturedTarget(plane, target_id) &&
           fire_range_m > 0.0;
}

void TeamTacticController::HandleVolleyFire(
    PlaneState_S* planes,
    int count,
    const PlaneState_S& target,
    int step) {
    if (plan_.primary_attackers.empty()) {
        plan_.sync_wait_steps = 0;
        plan_.sync_wait_plane_id = -1;
        return;
    }

    struct ReadyPlane {
        int plane_id = -1;
        double distance_m = 0.0;
    };
    std::vector<ReadyPlane> ready_planes;
    std::vector<int> armed_attackers;

    for (int plane_id : plan_.primary_attackers) {
        PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
        if (plane == nullptr || plane->_isAlive <= 0) {
            continue;
        }
        PlaneControlState& state = plan_.plane_states[plane_id];
        if (state.dodge.active) {
            continue;
        }
        if (plane->_missileCount > 0) {
            armed_attackers.push_back(plane_id);
        }

        const double distance = GeoDistanceMeters(
            plane->_longitude,
            plane->_latitude,
            target._longitude,
            target._latitude);
        const double fire_threshold = std::max(
            22000.0,
            plan_.profile.first_fire_range - plan_.profile.round_range_interval * state.fired_rounds);
        if (distance <= fire_threshold && CanPlaneFireNow(*plane, state, plan_.target_id, fire_threshold)) {
            ready_planes.push_back({plane_id, distance});
        }
    }

    if (armed_attackers.size() >= 2 && ready_planes.size() == 1) {
        if (plan_.sync_wait_plane_id == ready_planes.front().plane_id) {
            ++plan_.sync_wait_steps;
        } else {
            plan_.sync_wait_plane_id = ready_planes.front().plane_id;
            plan_.sync_wait_steps = 1;
        }
    } else if (ready_planes.empty()) {
        plan_.sync_wait_steps = 0;
        plan_.sync_wait_plane_id = -1;
    }

    bool allow_single_ready_fire = false;
    if (armed_attackers.size() <= 1) {
        allow_single_ready_fire = true;
    } else if (ready_planes.size() >= 2) {
        allow_single_ready_fire = true;
    } else if (ready_planes.size() == 1) {
        const int ready_id = ready_planes.front().plane_id;
        int other_id = -1;
        for (int plane_id : armed_attackers) {
            if (plane_id != ready_id) {
                other_id = plane_id;
                break;
            }
        }
        const bool other_compromised =
            other_id == -1 ||
            plan_.plane_states[other_id].dodge.active ||
            plan_.plane_states[other_id].last_threat.severe;
        allow_single_ready_fire = other_compromised || plan_.sync_wait_steps >= static_cast<int>(kFireReadyHoldTimeoutSteps);
    }

    if (ready_planes.empty() || !allow_single_ready_fire) {
        return;
    }

    for (const ReadyPlane& ready : ready_planes) {
        PlaneState_S* plane = FindPlaneById(planes, count, ready.plane_id);
        if (plane == nullptr || plane->_missileCount <= 0) {
            continue;
        }

        PlaneControlState& state = plan_.plane_states[ready.plane_id];
        plane->_radarState = RadarState_E::Track;
        plane->_targetID = plan_.target_id;
        plane->_isShoot = true;
        ++state.fired_rounds;
        state.cooldown_steps = 25;

        std::cout
            << "[TACTIC] team=" << team_name_
            << " | plane=" << ready.plane_id
            << " | fire"
            << " | target=" << plan_.target_id
            << " | dist_m=" << ready.distance_m
            << " | salvo_round=" << state.fired_rounds
            << std::endl;
    }

    plan_.sync_wait_steps = 0;
    plan_.sync_wait_plane_id = -1;
}

const char* TeamTacticController::RoleName(PlaneRole role) const {
    switch (role) {
    case PlaneRole::Lead:
        return "LEAD";
    case PlaneRole::Wing:
        return "WING";
    case PlaneRole::Decoy:
        return "DECOY";
    case PlaneRole::Escort:
        return "ESCORT";
    case PlaneRole::Return:
        return "RTB";
    case PlaneRole::Evade:
        return "EVADE";
    default:
        return "IDLE";
    }
}

std::string TeamTacticController::BuildTacviewLabel(const PlaneState_S& plane, const PlaneControlState& state) const {
    std::ostringstream oss;
    oss << RoleName(state.role)
        << " M" << std::max(0, plane._missileCount)
        << " W" << state.last_threat.warning_count
        << " I" << state.last_threat.inbound_missile_count;
    return oss.str();
}

void TeamTacticController::Apply(PlaneState_S* planes, int count, int step, double dt) {
    if (plan_.objective_complete) {
        for (int plane_id : plan_.team_plane_ids) {
            PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
            if (plane == nullptr || plane->_isAlive <= 0) {
                continue;
            }
            PlaneControlState& state = plan_.plane_states[plane_id];
            state.role = PlaneRole::Return;
            ApplyReturnBehavior(*plane, state, dt);
            std::snprintf(plane->_pilot, sizeof(plane->_pilot), "%s", BuildTacviewLabel(*plane, state).c_str());
        }
        return;
    }

    const PlaneState_S* current_target = FindPlaneById(planes, count, plan_.target_id);
    if (plan_.target_id == -1 || current_target == nullptr || current_target->_isAlive <= 0) {
        RefreshPlan(planes, count, step, false, 0, 0, "apply");
    }

    AssignRoles(planes, count);
    const PlaneState_S* target = FindPlaneById(planes, count, plan_.target_id);

    for (int plane_id : plan_.team_plane_ids) {
        PlaneState_S* plane = FindPlaneById(planes, count, plane_id);
        if (plane == nullptr || plane->_isAlive <= 0) {
            continue;
        }

        PlaneControlState& state = plan_.plane_states[plane_id];
        state.last_threat = AnalyzeThreatForPlane(*plane, planes, count);

        const bool is_primary_attacker =
            std::find(plan_.primary_attackers.begin(), plan_.primary_attackers.end(), plane_id) != plan_.primary_attackers.end();
        const bool is_decoy =
            std::find(plan_.decoys.begin(), plan_.decoys.end(), plane_id) != plan_.decoys.end();

        if (is_decoy) {
            state.role = PlaneRole::Decoy;
            MaybeActivateDodge(*plane, state, state.last_threat, step, true);
        } else if (is_primary_attacker) {
            state.role = (plane_id == plan_.primary_attackers.front()) ? PlaneRole::Lead : PlaneRole::Wing;
            MaybeActivateDodge(*plane, state, state.last_threat, step, false);
        } else if (plane->_missileCount > 0) {
            state.role = PlaneRole::Escort;
        } else {
            state.role = PlaneRole::Return;
        }

        if (state.dodge.active) {
            ApplyDodgeBehavior(*plane, state, state.last_threat, step, dt);
        } else if (is_primary_attacker && target != nullptr && plane->_missileCount > 0) {
            const int attacker_index = static_cast<int>(
                std::find(plan_.primary_attackers.begin(), plan_.primary_attackers.end(), plane_id) - plan_.primary_attackers.begin());
            ApplyAttackBehavior(*plane, state, *target, attacker_index, static_cast<int>(plan_.primary_attackers.size()), dt);
        } else if (is_decoy && target != nullptr) {
            const int decoy_index = static_cast<int>(
                std::find(plan_.decoys.begin(), plan_.decoys.end(), plane_id) - plan_.decoys.begin());
            ApplyDecoyBehavior(*plane, state, *target, decoy_index, static_cast<int>(plan_.decoys.size()), dt);
        } else {
            state.role = PlaneRole::Return;
            ApplyReturnBehavior(*plane, state, dt);
        }

        std::snprintf(plane->_pilot, sizeof(plane->_pilot), "%s", BuildTacviewLabel(*plane, state).c_str());
    }

    if (target != nullptr && target->_isAlive > 0) {
        HandleVolleyFire(planes, count, *target, step);
    }
}

TeamTelemetry TeamTacticController::GetTelemetry(const PlaneState_S* planes, int count) const {
    TeamTelemetry telemetry;
    telemetry.target_id = plan_.target_id;
    telemetry.planned_salvos = plan_.planned_salvos;
    telemetry.objective_complete = plan_.objective_complete;
    telemetry.side = plan_.profile.side;
    telemetry.primary_attacker_count = static_cast<int>(plan_.primary_attackers.size());
    telemetry.decoy_count = static_cast<int>(plan_.decoys.size());
    telemetry.fired_total = std::max(0, plan_.initial_team_missiles - SumTeamMissiles(planes, count));
    return telemetry;
}

int TeamTacticController::target_id() const {
    return plan_.target_id;
}

bool TeamTacticController::objective_complete() const {
    return plan_.objective_complete;
}

ZhanShu::SurroundSide TeamTacticController::side() const {
    return plan_.profile.side;
}

} // namespace EncounterBatchTactics
