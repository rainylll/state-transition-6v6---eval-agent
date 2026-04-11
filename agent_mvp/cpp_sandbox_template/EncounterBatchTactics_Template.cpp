#include "EncounterBatchTactics_Template.h"

#include <cmath>
#include <limits>

namespace EncounterBatchTemplate {

namespace {

double DegToRad(double deg)
{
    return deg * 3.14159265358979323846 / 180.0;
}

double GeoDistanceMeters(
    double lon1,
    double lat1,
    double lon2,
    double lat2)
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

} // namespace

TemplateTacticController::TemplateTacticController(int team) : team_(team)
{
}

void TemplateTacticController::StartTask(const TacticalTaskConfig& task)
{
    current_task_ = task;

    // RL teams can load per-task or per-scenario assets here if needed.
    // Example:
    // use_rl_policy_ = (task.tactic_id == 99);
}

std::vector<UnitCommand> TemplateTacticController::BuildStepCommands(const TeamStepInput& input)
{
    if (UseRlPolicy()) {
        std::vector<UnitCommand> commands;
        const UnitObservation* focus_target = SelectFocusTarget(input);
        for (const UnitObservation& self : input.self_units) {
            if (!self.alive) {
                continue;
            }
            commands.push_back(BuildRlInferenceCommand(self, input, focus_target));
        }
        return commands;
    }

    return BuildRuleBasedCommands(input);
}

void TemplateTacticController::FinishTask()
{
    // Tactical teams can flush logs, release model handles, or export
    // diagnostics here. The runner owns episodes.jsonl writing.
}

bool TemplateTacticController::UseRlPolicy() const
{
    return use_rl_policy_;
}

std::vector<UnitCommand> TemplateTacticController::BuildRuleBasedCommands(const TeamStepInput& input) const
{
    std::vector<UnitCommand> commands;
    const UnitObservation* focus_target = SelectFocusTarget(input);

    for (const UnitObservation& self : input.self_units) {
        if (!self.alive) {
            continue;
        }

        const UnitObservation* nearest_enemy = SelectNearestEnemy(self, input.enemy_units);
        const UnitObservation* target = (nearest_enemy != nullptr) ? nearest_enemy : focus_target;
        if (target == nullptr) {
            continue;
        }

        UnitCommand command;
        command.plane_id = self.plane_id;
        command.target_id = target->plane_id;
        command.desired_lon = target->lon;
        command.desired_lat = target->lat;
        command.desired_alt = target->alt;
        command.desired_speed = (self.missile_count > 0) ? 320.0 : 280.0;
        command.radar_state = RadarState_E::Track;

        const double distance_m = GeoDistanceMeters(
            self.lon,
            self.lat,
            target->lon,
            target->lat);

        if (self.missile_count > 0) {
            command.debug_role = "TEMPLATE_ATTACK";
            command.fire = self.target_locked && distance_m <= 60000.0;
        } else {
            // This is intentionally simple. Tactical teams should replace this
            // with their own screening, decoy, or RL-based behavior.
            command.debug_role = "TEMPLATE_SCREEN";
            command.evade = (self.inbound_missile_count > 0 || self.warning_count > 0);
            command.desired_alt = target->alt + 500.0;
        }

        if (self.inbound_missile_count > 0) {
            command.debug_role = "TEMPLATE_EVADE";
            command.evade = true;
            command.fire = false;
            command.desired_speed = 340.0;
        }

        commands.push_back(command);
    }

    return commands;
}

UnitCommand TemplateTacticController::BuildRlInferenceCommand(
    const UnitObservation& self,
    const TeamStepInput& input,
    const UnitObservation* focus_target) const
{
    UnitCommand command;
    command.plane_id = self.plane_id;
    command.debug_role = "TEMPLATE_RL";

    const UnitObservation* target = focus_target;
    if (target == nullptr) {
        target = SelectNearestEnemy(self, input.enemy_units);
    }
    if (target == nullptr) {
        return command;
    }

    // Placeholder behavior: keep the I/O shape stable, but mirror the rule
    // baseline until an actual policy is wired in.
    command.target_id = target->plane_id;
    command.desired_lon = target->lon;
    command.desired_lat = target->lat;
    command.desired_alt = target->alt;
    command.desired_speed = 320.0;
    command.radar_state = RadarState_E::Track;
    command.fire = self.missile_count > 0 && self.target_locked;
    return command;
}

const UnitObservation* TemplateTacticController::SelectFocusTarget(const TeamStepInput& input) const
{
    for (const UnitObservation& enemy : input.enemy_units) {
        if (enemy.alive) {
            return &enemy;
        }
    }
    return nullptr;
}

const UnitObservation* TemplateTacticController::SelectNearestEnemy(
    const UnitObservation& self,
    const std::vector<UnitObservation>& enemies) const
{
    const UnitObservation* best = nullptr;
    double best_distance = std::numeric_limits<double>::max();
    for (const UnitObservation& enemy : enemies) {
        if (!enemy.alive) {
            continue;
        }
        const double distance_m = GeoDistanceMeters(
            self.lon,
            self.lat,
            enemy.lon,
            enemy.lat);
        if (distance_m < best_distance) {
            best_distance = distance_m;
            best = &enemy;
        }
    }
    return best;
}

} // namespace EncounterBatchTemplate
