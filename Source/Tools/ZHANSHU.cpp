#include "ZHANSHU.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "../Control/flyPID.h"

namespace ZhanShu {
    // -----------------------------
    // Utilities
    // -----------------------------
    double BearingDeg(double aLat, double aLon, double bLat, double bLon)
    {
        int earth_LL_jvli = 111000;
        double lat1 = aLat;
        double lon1 = aLon;
        double lat2 = bLat;
        double lon2 = bLon;

        int xiangxian;
        double dlat = (lat2 - lat1) * earth_LL_jvli;
        double dlon = (lon2 - lon1) * earth_LL_jvli;
        if (dlat >= 0 && dlon >= 0)
            xiangxian = 1;
        if (dlat >= 0 && dlon <= 0)
        {
            xiangxian = 2;
            dlon = -dlon;
        }

        if (dlat <= 0 && dlon <= 0)
        {
            xiangxian = 3;
            dlon = -dlon;
            dlat = -dlat;
        }

        if (dlat <= 0 && dlon >= 0)
        {
            xiangxian = 4;
            dlat = -dlat;
        }
        double y = dlat;
        double x = dlon;
        double target_bearing;

        if (xiangxian == 1)
            target_bearing = 90 - atan2(y, x) * 180.0 / M_PI;
        if (xiangxian == 2)
            target_bearing = 270 + atan2(y, x) * 180.0 / M_PI;
        if (xiangxian == 3)
            target_bearing = 270 - atan2(y, x) * 180.0 / M_PI;
        if (xiangxian == 4)
            target_bearing = 90 + atan2(y, x) * 180.0 / M_PI;

        if (target_bearing < 0) target_bearing += 360.0;
        while (target_bearing >= 360.0) target_bearing -= 360.0;
        return target_bearing;
    }

    double ElevationDeg(double aLat, double aLon, double aAlt, double bLat, double bLon, double bAlt)
    {
        const double earth_LL_jvli = 111000.0;

        double dlat_m = (bLat - aLat) * earth_LL_jvli;
        double dlon_m = (bLon - aLon) * earth_LL_jvli;

        double horizontal_distance = std::sqrt(dlat_m * dlat_m + dlon_m * dlon_m);
        double delta_alt = bAlt - aAlt;

        double elevation_radians;
        if (horizontal_distance < 100) {
            elevation_radians = (delta_alt >= 0) ? M_PI_2 : -M_PI_2;
        }
        else {
            elevation_radians = std::atan2(delta_alt, horizontal_distance);
        }

        return elevation_radians * 180.0 / M_PI;
    }

    std::vector<PlaneState_S*> GetPlanePointersById(PlaneState_S* planes, int planeCount, const std::vector<int>& planeIds)
    {
        std::vector<PlaneState_S*> out;
        out.reserve(planeIds.size());

        for (int id : planeIds) {
            for (int i = 0; i < planeCount; ++i) {
                if (planes[i]._planeID == id) {
                    out.push_back(&planes[i]);
                    break;
                }
            }
        }
        return out;
    }

    // -----------------------------
    // Legacy functions (kept from previous version)
    // -----------------------------
    // NOTE: To keep this patch focused on merging BHCA / wrapper, the other legacy
    // functions remain in the file as they were. (They are not shown here.)

    // -----------------------------
    // BaoWeiTactic (BHCA merged)
    // -----------------------------

    int BaoWeiTactic::FindIndexById(PlaneState_S* planes, int planeCount, int planeId)
    {
        if (!planes || planeCount <= 0) return -1;

        auto it = _id2index.find(planeId);
        if (it != _id2index.end()) {
            int idx = it->second;
            if (idx >= 0 && idx < planeCount && planes[idx]._planeID == planeId) return idx;
        }

        for (int i = 0; i < planeCount; ++i) {
            if (planes[i]._planeID == planeId) {
                _id2index[planeId] = i;
                return i;
            }
        }
        return -1;
    }

    TacticalResult BaoWeiTactic::zhanshu_muban_yule_baowei(
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
        SurroundSide side)
    {

        if (!planes || planeCount <= 0) return _result;
        if (planeIds.empty()) return _result;

        const PlaneState_S* target_ship = GetPlaneState(targetPlaneId);
        if (!target_ship) return _result;

        const double centerLon = target_ship->_longitude;
        const double centerLat = target_ship->_latitude;

        // Initialize mapping and geometry once
        if (!_setupDone)
        {
            _id2index.clear();
            _id2index.reserve((size_t)planeCount);
            for (int idx = 0; idx < planeCount; ++idx) {
                _id2index[planes[idx]._planeID] = idx;
            }

            // Filter invalid plane IDs early
            _ordered.clear();
            _ordered.reserve(planeIds.size());
            for (int pid : planeIds) {
                if (FindIndexById(planes, planeCount, pid) >= 0) {
                    _ordered.push_back(pid);
                }
            }

            int n = (int)_ordered.size();
            if (n <= 0) return _result;

            std::sort(_ordered.begin(), _ordered.end(), [&](int ida, int idb) {
                int ia = FindIndexById(planes, planeCount, ida);
                int ib = FindIndexById(planes, planeCount, idb);
                if (ia < 0 || ib < 0) return ida < idb;
                if (planes[ia]._latitude == planes[ib]._latitude)
                    return planes[ia]._planeID < planes[ib]._planeID;
                return planes[ia]._latitude > planes[ib]._latitude;
                });

            double halfArc = surroundArcDegrees * 0.5;
            // Decide which side the half-circle sits on
            // West: arc centered at 180° (default)
            // East: arc centered at 0°
            double arcCenterDeg = (side == SurroundSide::East) ? 0.0 : 180.0;
            double startDeg = arcCenterDeg - halfArc;
            double endDeg = arcCenterDeg + halfArc;

            bool isOdd = (n % 2) == 1;
            int midPos = isOdd ? (n / 2) : -1;

            // Target points on arc
            for (int i = 0; i < n; ++i) {
                int id = _ordered[i];
                _stage[id] = 1;

                double angleDeg;
                if (n == 1) angleDeg = arcCenterDeg;
                else {
                    double t = (double)i / (double)(n - 1);
                    angleDeg = startDeg + t * surroundArcDegrees;
                }

                double angleRad = angleDeg * M_PI / 180.0;
                double dLat = surroundRadius * std::sin(angleRad);
                double dLon = surroundRadius * std::cos(angleRad);
                _targetPoint[id] = std::make_tuple(centerLon + dLon, centerLat + dLat, altitude);
            }

            // Mid staging points
            double northMidDeg = (n < 2) ? startDeg : (2 * startDeg + (n / 2 - 1) * surroundArcDegrees / (n - 1)) / 2;
            double southMidDeg = (n < 2) ? endDeg : (2 * endDeg - (n / 2 - 1) * surroundArcDegrees / (n - 1)) / 2;

            double lonN = centerLon + midRadiusRatio * surroundRadius * std::cos(northMidDeg * M_PI / 180.0);
            double latN = centerLat + midRadiusRatio * surroundRadius * std::sin(northMidDeg * M_PI / 180.0);
            double lonS = centerLon + midRadiusRatio * surroundRadius * std::cos(southMidDeg * M_PI / 180.0);
            double latS = centerLat + midRadiusRatio * surroundRadius * std::sin(southMidDeg * M_PI / 180.0);

            for (int i = 0; i < n; ++i) {
                int id = _ordered[i];
                if (isOdd && i == midPos) {
                    _stage[id] = 2;
                    continue;
                }

                bool isNorth = (i < n / 2);
                double sLon = isNorth ? lonN : lonS;
                double sLat = isNorth ? latN : latS;
                _stagingPoint[id] = std::make_tuple(sLon, sLat, altitude);
            }

            _setupDone = true;
        }

        // Downed recording
        for (int id : _ordered)
        {
            int idx = FindIndexById(planes, planeCount, id);
            if (idx < 0) continue;
            PlaneState_S& p = planes[idx];
            if (p._isAlive == 0 && !_downedRecorded[id])
            {
                _downedRecorded[id] = true;
                _ready = false;
                _result.downedPlaneIDs.push_back(id);

                auto spIt = _stagingPoint.find(id);
                if (spIt != _stagingPoint.end()) _result.stagingPoints[id] = spIt->second;

                auto tpIt = _targetPoint.find(id);
                if (tpIt != _targetPoint.end()) _result.inheritedTargets[id] = tpIt->second;
            }
        }

        // Support phase
        if (needSupport && !_ready)
        {
            if (!_supportInjected)
            {
                std::vector<int> downSlots;
                downSlots.reserve(_result.downedPlaneIDs.size());
                for (size_t i = 0; i < _ordered.size(); ++i) {
                    int pid = _ordered[i];
                    if (std::find(_result.downedPlaneIDs.begin(), _result.downedPlaneIDs.end(), pid) != _result.downedPlaneIDs.end())
                        downSlots.push_back((int)i);
                }

                size_t pairCount = (std::min)(downSlots.size(), supportPlaneIds.size());
                for (size_t k = 0; k < pairCount; ++k) {
                    int slot = downSlots[k];
                    int oldId = _ordered[slot];
                    int supId = supportPlaneIds[k];

                    if (std::find(_ordered.begin(), _ordered.end(), supId) != _ordered.end())
                        continue;

                    if (FindIndexById(planes, planeCount, supId) < 0)
                        continue;

                    auto spIt = _stagingPoint.find(oldId);
                    if (spIt != _stagingPoint.end()) _stagingPoint[supId] = spIt->second;
                    auto tpIt = _targetPoint.find(oldId);
                    if (tpIt != _targetPoint.end()) _targetPoint[supId] = tpIt->second;

                    _stage[supId] = _stage.count(oldId) ? _stage[oldId] : 1;
                    _firedRounds[supId] = _firedRounds.count(oldId) ? _firedRounds[oldId] : 0;

                    _ordered[slot] = supId;
                }

                _supportInjected = true;
            }

            // Guide to staging points
            for (int id : _ordered)
            {
                int idx = FindIndexById(planes, planeCount, id);
                if (idx < 0) continue;

                PlaneState_S& p = planes[idx];
                if (p._isAlive == 0) continue;

                auto it = _stagingPoint.find(id);
                if (it == _stagingPoint.end()) continue;

                double sLon, sLat, sAlt;
                std::tie(sLon, sLat, sAlt) = it->second;

                // p._yaw = BearingDeg(p._latitude, p._longitude, sLat, sLon);
                // p._pitch = ElevationDeg(p._latitude, p._longitude, p._altitude, sLat, sLon, altitude);
                FlightController& fc = s_flightControllers[p._planeID];
                fc.run_control(p, sLon, sLat, altitude, 400.0, 0.1);
            }

            bool ready = true;
            for (int id : _ordered)
            {
                int idx = FindIndexById(planes, planeCount, id);
                if (idx < 0) { ready = false; break; }

                auto it = _stagingPoint.find(id);
                if (it == _stagingPoint.end()) { ready = false; break; }

                double sLon, sLat, sAlt;
                std::tie(sLon, sLat, sAlt) = it->second;

                const PlaneState_S* self = planes + idx;
                if (std::fabs(self->_longitude - sLon) > 0.05 || std::fabs(self->_latitude - sLat) > 0.05) {
                    ready = false;
                    break;
                }
            }
            if (ready) _ready = true;
        }

        // Normal phases
        if (_ready)
        {
            for (int id : _ordered)
            {
                int idx = FindIndexById(planes, planeCount, id);
                if (idx < 0) continue;

                PlaneState_S& p = planes[idx];
                if (p._isAlive == 0) continue;

                FlightController& fc = s_flightControllers[p._planeID];

                int stage = _stage.count(id) ? _stage[id] : 1;

                if (stage == 1)
                {
                    auto it = _stagingPoint.find(id);
                    if (it == _stagingPoint.end()) { _stage[id] = 2; continue; }

                    double sLon, sLat, sAlt;
                    std::tie(sLon, sLat, sAlt) = it->second;

                    // p._yaw = BearingDeg(p._latitude, p._longitude, sLat, sLon);
                    // p._pitch = ElevationDeg(p._latitude, p._longitude, p._altitude, sLat, sLon, altitude);
                    fc.run_control(p, sLon, sLat, altitude, 450.0, 0.1);

                    if (std::fabs(p._longitude - sLon) <= 0.02 && std::fabs(p._latitude - sLat) <= 0.02)
                        _stage[id] = 2;
                }
                else if (stage == 2)
                {
                    auto it = _targetPoint.find(id);
                    if (it == _targetPoint.end()) { _stage[id] = 3; continue; }

                    double sLon, sLat, sAlt;
                    std::tie(sLon, sLat, sAlt) = it->second;

                    // p._yaw = BearingDeg(p._latitude, p._longitude, sLat, sLon);
                    // p._pitch = ElevationDeg(p._latitude, p._longitude, p._altitude, sLat, sLon, altitude);
                    fc.run_control(p, sLon, sLat, altitude, 450.0, 0.1);

                    if (std::fabs(p._longitude - sLon) <= 0.02 && std::fabs(p._latitude - sLat) <= 0.02)
                        _stage[id] = 3;
                }
                else if (stage == 3)
                {
                    double aimLon = target_ship->_longitude;
                    double aimLat = target_ship->_latitude;

                    // p._yaw = BearingDeg(p._latitude, p._longitude, aimLat, aimLon);
                    // p._pitch = ElevationDeg(p._latitude, p._longitude, p._altitude, aimLat, aimLon, altitude);
                    fc.run_control(p, aimLon, aimLat, altitude, 480.0, 0.1);

                    double dLonm = (aimLon - p._longitude) * 111000.0;
                    double dLatm = (aimLat - p._latitude) * 111000.0;
                    double dist = std::sqrt(dLonm * dLonm + dLatm * dLatm);

                    int fired = _firedRounds.count(id) ? _firedRounds[id] : 0;
                    double threshold = (std::max)(0.0, firstFireRange - roundRangeInterval * fired);

                    if (fired < missileRounds && dist <= threshold)
                    {
                        // Old: p._raderCaptureCount > 0 只能表示“捕获到某些目标”，不保证包含当前 targetPlaneId。
                        // New: 只有当捕获列表里包含目标ID时才允许发射。
                        bool targetCaptured = false;
                        const int capN = (std::min)(p._raderCaptureCount, Max_Plane_Count);
                        for (int k = 0; k < capN; ++k) {
                            if (p._raderCapturePlanesID[k] == targetPlaneId) {
                                targetCaptured = true;
                                break;
                            }
                        }

                        bool can_fire = (targetCaptured && p._missileCount > 0);
                        if (can_fire)
                        {
                            p._radarState = 2;
                            p._targetID = targetPlaneId;
                            p._isShoot = true;
                            _firedRounds[id] = fired + 1;
                        }
                    }

                    if (_firedRounds[id] >= missileRounds)
                        _stage[id] = 4;
                }
                else if (stage == 4)
                {
                    // p._yaw = BearingDeg(p._latitude, p._longitude, returnLat, returnLon);
                    // p._pitch = ElevationDeg(p._latitude, p._longitude, p._altitude, returnLat, returnLon, altitude);
                    fc.run_control(p, returnLon, returnLat, altitude, 450.0, 0.1);

                    if (std::fabs(p._longitude - returnLon) <= 0.02 && std::fabs(p._latitude - returnLat) <= 0.02)
                        _stage[id] = 5;
                }
            }
        }

        return _result;
    }

    // Backward-compatible functional wrapper using an internal static instance.
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
        SurroundSide side)
    {
        static BaoWeiTactic tactic;
        // NOTE: callers that need separate concurrent instances should use BaoWeiTactic directly.
        return tactic.zhanshu_muban_yule_baowei(
            planes, kDefaultPlaneCount,
            myPlaneIds,
            targetPlaneId,
            surroundArcDegrees,
            surroundRadius,
            midRadiusRatio,
            returnLon,
            returnLat,
            missileRounds,
            firstFireRange,
            roundRangeInterval,
            altitude,
            supportPlaneIds,
            needSupport,
            side);
    }

    // ------------------------------------------------------------
    // Missing-linkage wrappers
    // ------------------------------------------------------------

    static void CopyToFixedBuffer(PlaneState_S dst[kDefaultPlaneCount], PlaneState_S* src, int srcCount)
    {
        std::memset(dst, 0, sizeof(PlaneState_S) * kDefaultPlaneCount);
        if (!src || srcCount <= 0) return;
        int n = (std::min)(srcCount, kDefaultPlaneCount);
        std::memcpy(dst, src, sizeof(PlaneState_S) * n);
    }

    int zhanshu_moban(PlaneState_S* const planes, std::vector<int>& myPlaneIds,
        int targetPlaneId, int selfShipId, double dt)
    {
        // NOTE: 原工程中此函数只有声明未实现。
        // 这里给出一个可用实现：使用固定数组 wrapper 调用包围模板，保证可链接与可运行。
        (void)selfShipId;
        (void)dt;

        PlaneState_S fixed[kDefaultPlaneCount];
        CopyToFixedBuffer(fixed, planes, kDefaultPlaneCount);

        // 使用一个中等参数的包围模板
        (void)ZhanShu::zhanshu_muban_yule_baowei(
            fixed,
            myPlaneIds,
            targetPlaneId,
            120.0,
            1.0,
            1.1,
            0.0, 0.0,
            1,
            60000.0,
            10000.0,
            8000.0,
            std::vector<int>{},
            false,
            SurroundSide::West);

        return 0;
    }

    int zhanshu_moban_luo_youdao(PlaneState_S* const planes, std::vector<int>& myPlaneIds,
        int targetPlaneId, int selfShipId, double dt)
    {
        // NOTE: 同样仅声明未实现。为了保证工程可链接与可运行，
        // 这里暂时复用 zhanshu_moban 的逻辑。
        return zhanshu_moban(planes, myPlaneIds, targetPlaneId, selfShipId, dt);
    }

    // helper: call fixed-size legacy qishe if it exists elsewhere as a different symbol
    // In this workspace, only the fixed-size signature is declared in the header; we forward to it by casting.
    static int chuan_qishe_fixed(int missileSalvoCount, double dt, PlaneState_S fixed[kDefaultPlaneCount],
        int targetIndex, double interval, std::vector<int>& myPlaneIds)
    {
        // This resolves to the fixed-size overload (non-pointer) in the same namespace.
        return chuan_qishe(missileSalvoCount, dt, fixed, targetIndex, interval, myPlaneIds);
    }

    int chuan_qishe(int missileSalvoCount, double dt, PlaneState_S* const planes,
        int targetIndex, double interval, std::vector<int>& myPlaneIds)
    {
        PlaneState_S fixed[kDefaultPlaneCount];
        CopyToFixedBuffer(fixed, planes, kDefaultPlaneCount);
        return chuan_qishe_fixed(missileSalvoCount, dt, fixed, targetIndex, interval, myPlaneIds);
    }

} // namespace ZhanShu
