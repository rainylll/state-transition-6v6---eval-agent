#pragma once

// Standard headers first (avoid macro pollution from engine headers)
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "../Control/flyPID.h"

// If some engine header defines `unordered_map` or other tokens as macros, undefine here.
#ifdef unordered_map
#undef unordered_map
#endif

#include "../Tools/interface.h"
#include "../Tools/WaypointControl.h"

namespace ZhanShu {

    constexpr int kDefaultPlaneCount = 12;

    // 包围方向：决定半圆弧线在目标的哪一侧
    // West: 以 180° 为弧线中心（默认，目标西侧）
    // East: 以 0° 为弧线中心（目标东侧）
    enum class SurroundSide {
        West = 0,
        East = 1,
    };

    struct TacticalResult {
        std::vector<int> downedPlaneIDs;
        std::map<int, std::tuple<double, double, double>> stagingPoints;
        std::map<int, std::tuple<double, double, double>> inheritedTargets;
    };

    // --- Utilities ---
    double BearingDeg(double aLat, double aLon, double bLat, double bLon);
    double ElevationDeg(double aLat, double aLon, double aAlt, double bLat, double bLon, double bAlt);

    std::vector<PlaneState_S*> GetPlanePointersById(PlaneState_S* planes, int planeCount, const std::vector<int>& planeIds);

    // --- Legacy scenario init ---
    int initialization_all(int targetIndex, PlaneState_S planes[kDefaultPlaneCount]);

    // --- Legacy tactics (kept for compatibility) ---
    int zhanshu__baowei(PlaneState_S planes[kDefaultPlaneCount], int targetIndex,
        bool waypointSet[36], bool waypointFinished[36], bool hasFired[36],
        WaypointControl waypointcontrols[36], double dt = 0);

    int zhanshu_youdao(PlaneState_S planes[kDefaultPlaneCount], int targetIndex,
        bool waypointSet[36], bool waypointFinished[36], bool hasFired[36],
        WaypointControl waypointcontrols[36], double dt = 0);

    int chuan_qishe(int missileSalvoCount, double dt, PlaneState_S planes[kDefaultPlaneCount],
        int targetIndex, double interval, std::vector<int>& myPlaneIds);

    int zhanshu_moban(PlaneState_S planes[kDefaultPlaneCount], std::vector<int>& myPlaneIds,
        int targetPlaneId, int selfShipId, double dt);

    int zhanshu_moban_luo_youdao(PlaneState_S planes[kDefaultPlaneCount], std::vector<int>& myPlaneIds,
        int targetPlaneId, int selfShipId, double dt);

    // -----------------------------
    // BHCA merged into a class tactic
    // -----------------------------
    class BaoWeiTactic {
    public:

        TacticalResult zhanshu_muban_yule_baowei(
            PlaneState_S* planes, int planeCount,
            const std::vector<int>& planeIds,
            int targetPlaneId,
            double surroundArcDegrees,
            double surroundRadius,
            double midRadiusRatio,
            double returnLon,
            double returnLat,
            int missileRounds,
            double firstFireRange,
            double roundRangeInterval,
            double altitude,
            const std::vector<int>& supportPlaneIds,
            bool needSupport,
            SurroundSide side = SurroundSide::West);

    private:
        int FindIndexById(PlaneState_S* planes, int planeCount, int planeId);

        bool _setupDone = false;
        bool _supportInjected = false;
        bool _ready = true;

        std::vector<int> _ordered;
        TacticalResult _result;

        std::unordered_map<int, int> _firedRounds;
        std::unordered_map<int, int> _stage;
        std::unordered_map<int, int> _id2index;
        std::unordered_map<int, bool> _downedRecorded;

        std::map<int, std::tuple<double, double, double>> _stagingPoint;
        std::map<int, std::tuple<double, double, double>> _targetPoint;

        std::map<int, FlightController> s_flightControllers;
    };

    // Backward-compatible functional wrapper using internal static instance.
    TacticalResult zhanshu_muban_yule_baowei(
        PlaneState_S planes[kDefaultPlaneCount],
        std::vector<int>& myPlaneIds,
        int targetPlaneId,
        double surroundArcDegrees,
        double surroundRadius,
        double midRadiusRatio,
        double returnLon,
        double returnLat,
        int missileRounds,
        double firstFireRange,
        double roundRangeInterval,
        double altitude,
        const std::vector<int>& supportPlaneIds,
        bool needSupport,
        SurroundSide side = SurroundSide::West);

} // namespace ZhanShu
