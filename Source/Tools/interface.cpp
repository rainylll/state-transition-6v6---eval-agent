#include "interface.h"
#define LOCKTARGETYAW_NOTFIND       999
#define LOCKTARGETPITCH_NOTFIND     998

#define SPEED_LIMIT_SET_MAX_COUNT   1000
#define SPEED_LIMIT_RATIO           0.1
#define HEIGHT_LIMIT_RATIO          0.1

extern bool SHOW_RADAR_ALL_HBEAMWIDTH;
extern bool SHOW_RADAR_ALL_VBEAMWIDTH;
extern bool SHOW_RADAR_ALL_RANGE;
extern bool SHOW_TACVIEW_RADAR;
extern Unit_Object_Limit* UNIT_OBJECT_LIMITS[];

typedef struct
{
    double  Azimuth;
    bool    isLeft;
} RadarLocation;

static JOYINFOEX                joyinfoex;
static int                      planeCount;
static double                   missStepTime; //东大团队添加
static Battlefield_C* battlefield;
static TacViewOutput* tacview_show;
static TacViewFile_T* acmiFile;
static PlaneState_S             planeStates[Max_Plane_Count];
static map<int, RadarLocation>  radarDataMap;
static int                      tempRaderCapturePlanesID[Max_Plane_Count];

typedef struct
{
    int     stepCount;

    int     yawAvgCount;
    int     rollAvgCount;
    int     pitchAvgCount;

    double  oldYaw;
    double  oldRoll;
    double  oldPitch;
    double  old_velocity_north;
    double  old_velocity_east;
    double  old_velocity_downward;

    double  yawDiffSum;
    double  rollDiffSum;
    double  pitchDiffSum;
} OverloadVar;

static OverloadVar overloadVar[Max_Plane_Count];

static void setRadarHGSynergyMode(PlaneState_S* selfPlane);
static void setRadarHGSynergyMode1(PlaneState_S* selfPlane);
static void setRadarHGSingleMode(PlaneState_S* selfPlane);
static void setRadarNormalMode(PlaneState_S* selfPlane);
static void setRadarESMSynergyMode(PlaneState_S* selfPlane);
static void execHGSynergyRadarScan(const int index);

void PlaneState_S::init(char limitIndex)
{
    _limitIndex = limitIndex;
    _roll_ctrl = YAW_BASE_SPEED;
    _pitch_ctrl = PITCH_BASE_SPEED;
    _yaw_ctrl = ROLL_BASE_SPEED;
    _throttle = DEFAULT_THROTTLE;

    _radarMode = RadarMode_E::RM_Normal;
    _equipmentType = FIGHTER;

    Unit_Object_Limit* limit = UNIT_OBJECT_LIMITS[_limitIndex];
    _radar_range = limit->radar_range;
    _radar_view_range = limit->radar_view_range;
    _radar_hBeamWidth = limit->radar_hBeamWidth;
    _radar_vBeamWidth = fabs(limit->radar_vBeamWidth_upper) + fabs(limit->radar_vBeamWidth_below);
    _radar_vBeamWidth_upper = limit->radar_vBeamWidth_upper;
    _radar_vBeamWidth_below = limit->radar_vBeamWidth_below;
    _radar_all_hBeamWidth = limit->radar_all_hbeamwidth;
    _radar_all_vBeamWidth = limit->radar_all_vbeamwidth;
    _radar_all_range = limit->radar_view_range;

}

//外部接口
const PlaneState_S* GetPlaneState(const int in_id)
{
    for (int i = 0; i < planeCount; i++)
        if (planeStates[i]._planeID == in_id)
            return &(planeStates[i]);

    return nullptr;
}

//弧度转角度
inline double angle(double rad)
{
    return rad * (180.0 / M_PI);
}

//通过飞机ID获得飞机状态索引
inline int getPlaneStateIndex(int planeID)
{
    for (int i = 0; i < planeCount; i++)
        if (planeStates[i]._planeID == planeID)
            return i;
    return -1;
}

//通过飞机ID获得外部飞机状态指针
inline PlaneState_S* getPlaneStateByID(const int in_TarID)
{
    for (int i = 0; i < planeCount; i++)
        if (planeStates[i]._planeID == in_TarID)
            return &(planeStates[i]);
    return nullptr;
}

//通过飞机ID获得外部飞机状态指针
inline PlaneState_S* getExternalPlaneStateByID(const int in_TarID, PlaneState_S* objs)
{
    int objsIndex = getPlaneStateIndex(objs->_planeID);
    if (objsIndex == -1)
        return nullptr;

    PlaneState_S* beginObj = objs - objsIndex;

    for (int i = 0; i < planeCount; i++)
        if (beginObj[i]._planeID == in_TarID)
            return &(beginObj[i]);
    return nullptr;
}

//通过飞机ID获得飞机对象指针
inline Aircraft_Object_C* getAircraftObjectByID(const int in_TarID)
{
    for (int i = 0; i < planeCount; i++)
        if (battlefield->aircraft_list[i].Sim_id == in_TarID)
            return &(battlefield->aircraft_list[i]);
    return nullptr;
}

static Eigen::Vector3d locationTarget(PlaneState_S* self, PlaneState_S* target)
{
    Eigen::Vector3d location = { 0, 0, 0 };
    if (self == nullptr || target == nullptr)
        return location;

    Matrix3d rMatrix;
    Vector3d xyz;
    double self_xyz[3] = { 0, 0, 0 };
    double target_xyz[3] = { 0, 0, 0 };

    rotation_navigation_to_body(&rMatrix, self->_roll, self->_pitch, self->_yaw);
    earth_to_navigation(&self_xyz[0], &self_xyz[1], &self_xyz[2],
        self->_longitude, self->_latitude, self->_altitude,
        target->_longitude, target->_latitude, target->_altitude);

    xyz << target_xyz[0] - self_xyz[0], target_xyz[1] - self_xyz[1], target_xyz[2] - self_xyz[2];
    xyz = rMatrix * xyz;

    location[2] = angle(atan2(xyz(1), xyz(0)));;
    location[1] = angle(-atan2(xyz(2), sqrt(pow(xyz(0), 2) + pow(xyz(1), 2))));;
    location[0] = 0;

    return location;
}

static Eigen::Vector3d locationTarget2(PlaneState_S* self, double tar_longitude, double tar_latitude, double tar_altitude)
{
    Eigen::Vector3d location = { 0, 0, 0 };
    if (self == nullptr)
        return location;

    Matrix3d rMatrix;
    Vector3d xyz;
    double self_xyz[3] = { 0, 0, 0 };
    double target_xyz[3] = { 0, 0, 0 };

    rotation_navigation_to_body(&rMatrix, self->_roll, self->_pitch, self->_yaw);
    earth_to_navigation(&self_xyz[0], &self_xyz[1], &self_xyz[2],
        self->_longitude, self->_latitude, self->_altitude,
        tar_longitude, tar_latitude, tar_altitude);

    xyz << target_xyz[0] - self_xyz[0], target_xyz[1] - self_xyz[1], target_xyz[2] - self_xyz[2];
    xyz = rMatrix * xyz;

    location[2] = angle(atan2(xyz(1), xyz(0)));;
    location[1] = angle(-atan2(xyz(2), sqrt(pow(xyz(0), 2) + pow(xyz(1), 2))));;
    location[0] = 0;

    return location;
}

static Eigen::Vector3d locationTarget3(double src_yaw, double src_pitch, double src_roll, double src_longitude, double src_latitude, double src_altitude, double tar_longitude, double tar_latitude, double tar_altitude)
{
    Eigen::Vector3d location = { 0, 0, 0 };
    Matrix3d rMatrix;
    Vector3d xyz;
    double self_xyz[3] = { 0, 0, 0 };
    double target_xyz[3] = { 0, 0, 0 };

    rotation_navigation_to_body(&rMatrix, src_roll, src_pitch, src_yaw);
    earth_to_navigation(&self_xyz[0], &self_xyz[1], &self_xyz[2],
        src_longitude, src_latitude, src_altitude,
        tar_longitude, tar_latitude, tar_altitude);

    xyz << target_xyz[0] - self_xyz[0], target_xyz[1] - self_xyz[1], target_xyz[2] - self_xyz[2];
    xyz = rMatrix * xyz;

    location[2] = angle(atan2(xyz(1), xyz(0)));;
    location[1] = angle(-atan2(xyz(2), sqrt(pow(xyz(0), 2) + pow(xyz(1), 2))));;
    location[0] = 0;

    return location;
}

//雷达动态扫描
static double getRadarUpdateRadarInfo(const PlaneState_S* ps)
{
    if (ps == nullptr)
        return 0;
    int in_planeID = ps->_planeID;
    //#define LeftMax       RADAR_ALL_HBEAMWIDTH / 2
    //#define RightMax  360 - RADAR_ALL_HBEAMWIDTH / 2
    int AzimuthWidth = 1.0;
    //int boundary = int(RADAR_ALL_HBEAMWIDTH) % int(RADAR_HBEAMWIDTH);
    //if ( boundary == 0 )
    //  boundary = RADAR_ALL_HBEAMWIDTH / 2 - RADAR_HBEAMWIDTH / 2;
    //else
    //  boundary = RADAR_ALL_HBEAMWIDTH - boundary / 2 - RADAR_HBEAMWIDTH / 2;

    if (ps->_radar_all_hBeamWidth == 360.0)
    {
        radarDataMap[in_planeID].Azimuth += AzimuthWidth;
        if (radarDataMap[in_planeID].Azimuth == 360.0)
            radarDataMap[in_planeID].Azimuth = 0.0;
    }
    else
    {
        int boundary = int(ps->_radar_all_hBeamWidth) % int(ps->_radar_hBeamWidth);
        if (boundary == 0)
            boundary = ps->_radar_all_hBeamWidth / 2 - ps->_radar_hBeamWidth / 2;
        else
            boundary = ps->_radar_all_hBeamWidth - boundary / 2 - ps->_radar_hBeamWidth / 2;

        if (radarDataMap[in_planeID].isLeft)
        {
            radarDataMap[in_planeID].Azimuth += AzimuthWidth;
            if (radarDataMap[in_planeID].Azimuth == 360.0)
                radarDataMap[in_planeID].Azimuth = 0.0;
            if (radarDataMap[in_planeID].Azimuth == boundary + 1)
                //if ( radarDataMap[in_planeID].Azimuth == LeftMax )
            {
                radarDataMap[in_planeID].isLeft = false;
                radarDataMap[in_planeID].Azimuth -= AzimuthWidth;
            }
        }
        else
        {
            radarDataMap[in_planeID].Azimuth -= AzimuthWidth;
            if (radarDataMap[in_planeID].Azimuth == 0.0)
                radarDataMap[in_planeID].Azimuth = 360.0;
            if (radarDataMap[in_planeID].Azimuth == 360 - boundary - 1)
                //if ( radarDataMap[in_planeID].Azimuth == RightMax )
            {
                radarDataMap[in_planeID].isLeft = true;
                radarDataMap[in_planeID].Azimuth += AzimuthWidth;
            }
        }
    }
    return radarDataMap[in_planeID].Azimuth;
}

//获得雷达捕获目标的偏航角
static double getLockTargetYaw(int index, int tarID, RadarMode_E rmode = RadarMode_E::RM_Normal)
{
    if (rmode < RadarMode_E::ESMSynergy && planeStates[index]._raderCaptureCount == 0)
        return LOCKTARGETYAW_NOTFIND;

    const Aircraft_Object_C* tarPlane = getAircraftObjectByID(tarID);
    if (tarPlane == nullptr || tarPlane->base_live == 0)
        return LOCKTARGETYAW_NOTFIND;

    for (int i = 0; rmode != RadarMode_E::ESMSynergy && i < planeStates[index]._raderCaptureCount; i++)
    {
        if (tarPlane->Sim_id == planeStates[index]._raderCapturePlanesID[i] && tarPlane->base_live == 1)
            break;

        if (i == planeStates[index]._raderCaptureCount - 1)
            return LOCKTARGETYAW_NOTFIND;
    }

    double lonf = tarPlane->coordinate_longitude - planeStates[index]._longitude;
    double latf = tarPlane->coordinate_latitude - planeStates[index]._latitude;
    double altf = tarPlane->coordinate_altitude - planeStates[index]._altitude;

    if (lonf == 0 && latf == 0)
        return LOCKTARGETYAW_NOTFIND;
    else if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude,
        planeStates[index]._longitude, planeStates[index]._latitude) <= planeStates[index]._radar_range)
    {
        Eigen::Vector3d location = locationTarget(&(planeStates[index]), getPlaneStateByID(tarPlane->Sim_id));
        double halfWidth = planeStates[index]._radar_all_hBeamWidth / 2;
        double halfHeight = planeStates[index]._radar_all_vBeamWidth / 2;

        if (location.z() > halfWidth || location.z() < -halfWidth
            || location.y() > planeStates[index]._radar_elevation + halfHeight
            || location.y() < planeStates[index]._radar_elevation + -halfHeight)
            //|| planeStates[index]._radar_elevation + location.y() > planeStates[index]._radar_vBeamWidth_upper
            //|| planeStates[index]._radar_elevation + location.y() < planeStates[index]._radar_vBeamWidth_below)
            return LOCKTARGETYAW_NOTFIND;
        else
            return location.z();
    }
    return LOCKTARGETYAW_NOTFIND;
}

static bool isInsideVertical(const PlaneState_S* selfPlane, const Aircraft_Object_C* tarPlane)
{
    double lonf = tarPlane->coordinate_longitude - selfPlane->_longitude;
    double latf = tarPlane->coordinate_latitude - selfPlane->_latitude;
    double altf = tarPlane->coordinate_altitude - selfPlane->_altitude;
    double distance = sqrt(pow(fabs(lonf), 2) + pow(fabs(latf), 2));

    //double vangle_1 = fabs(selfPlane->_radar_elevation) - selfPlane->_radar_vBeamWidth / 2;
    //double vangle_2 = selfPlane->_pitch + (signbit(selfPlane->_radar_elevation) ? -vangle_1 : vangle_1);
    double vangle_3 = angle(atan2(altf, distance * LatitudeToM));

    double vangle_4 = selfPlane->_radar_elevation + selfPlane->_radar_vBeamWidth_upper + selfPlane->_pitch;
    double vangle_5 = selfPlane->_radar_elevation + selfPlane->_radar_vBeamWidth_below + selfPlane->_pitch;

    //if ( vangle_2 < 0 && vangle_3 > 0 )
    //  return false;
    //else if ( vangle_2 > 0 && vangle_3 < 0 )
    //  return false;
    //else if ( vangle_2 < 0 && vangle_3 < 0 && selfPlane->_radar_elevation != 0 && vangle_3 > vangle_2 )
    //  return false;
    //else if ( vangle_2 > 0 && vangle_3 > 0 && selfPlane->_radar_elevation != 0 && vangle_3 < vangle_2 )
    //  return false;

    if (vangle_3 <= vangle_4 && vangle_3 >= vangle_5)
        return true;

    return false;
}

static void setOverload(PlaneState_S* selfPlane)
{
    bool signbit_a, signbit_b;
    double a1 = 0, b1 = 0, c1 = 0, d1 = 0, e1 = 0, f1 = 0, avgYaw = 1.0, avgRoll = 1.0, avgPitch = 1.0;
    double fabs_a, fabs_b;

    int planeIndex = getPlaneStateIndex(selfPlane->_planeID);
    if (planeIndex == -1)
        return;

    if (overloadVar[planeIndex].stepCount++ == 5)
    {
        if (overloadVar[planeIndex].yawAvgCount == 0 && overloadVar[planeIndex].oldYaw == 0)
            overloadVar[planeIndex].oldYaw = selfPlane->_yaw;
        if (overloadVar[planeIndex].rollAvgCount == 0 && overloadVar[planeIndex].oldRoll == 0)
            overloadVar[planeIndex].oldRoll = selfPlane->_roll;
        if (overloadVar[planeIndex].pitchAvgCount == 0 && overloadVar[planeIndex].oldPitch == 0)
            overloadVar[planeIndex].oldPitch = selfPlane->_pitch;

        signbit_a = signbit(selfPlane->_velocity_north);
        signbit_b = signbit(overloadVar[planeIndex].old_velocity_north);
        if (signbit_a == signbit_b)
        {
            if (signbit_b)
            {
                fabs_a = fabs(selfPlane->_velocity_north);
                fabs_b = fabs(overloadVar[planeIndex].old_velocity_north);
                if (fabs_a > fabs_b)
                    a1 = fabs_a - fabs_b;
                else
                    a1 = -(fabs_b - fabs_a);
            }
            else
                a1 = selfPlane->_velocity_north - overloadVar[planeIndex].old_velocity_north;
        }
        else
        {
            a1 = -fabs(selfPlane->_velocity_north - overloadVar[planeIndex].old_velocity_north);
            //a1 = fabs(fabs(planes[0]._velocity_north) - fabs(old_velocity_north));
        }

        signbit_a = signbit(selfPlane->_velocity_east);
        signbit_b = signbit(overloadVar[planeIndex].old_velocity_east);
        if (signbit_a == signbit_b)
        {
            if (signbit_b)
            {
                fabs_a = fabs(selfPlane->_velocity_east);
                fabs_b = fabs(overloadVar[planeIndex].old_velocity_east);
                if (fabs_a > fabs_b)
                    b1 = fabs_a - fabs_b;
                else
                    b1 = -(fabs_b - fabs_a);
            }
            else
            {
                b1 = selfPlane->_velocity_east - overloadVar[planeIndex].old_velocity_east;
            }
        }
        else
        {
            b1 = -fabs(selfPlane->_velocity_east - overloadVar[planeIndex].old_velocity_east);
            //b1 = fabs(fabs(planes[0]._velocity_east) - fabs(old_velocity_east));
        }

        signbit_a = signbit(selfPlane->_velocity_downward);
        signbit_b = signbit(overloadVar[planeIndex].old_velocity_downward);
        if (signbit_a == signbit_b)
        {
            if (signbit_b)
            {
                fabs_a = fabs(selfPlane->_velocity_downward);
                fabs_b = fabs(overloadVar[planeIndex].old_velocity_downward);
                if (fabs_a > fabs_b)
                    c1 = fabs_a - fabs_b;
                else
                    c1 = -(fabs_b - fabs_a);
            }
            else
            {
                c1 = selfPlane->_velocity_downward - overloadVar[planeIndex].old_velocity_downward;
            }
        }
        else
        {
            c1 = -fabs(selfPlane->_velocity_downward - overloadVar[planeIndex].old_velocity_downward);
            //c1 = fabs(fabs(planes[0]._velocity_downward) - fabs(old_velocity_downward));
        }

        d1 = BoundaryDiff(selfPlane->_yaw, overloadVar[planeIndex].oldYaw);
        //if (selfPlane->_yaw != overloadVar[planeIndex].oldYaw)
        //{
        //  overloadVar[planeIndex].yawAvgCount++;
        //  overloadVar[planeIndex].yawDiffSum += d1;
        //  avgYaw = overloadVar[planeIndex].yawDiffSum / overloadVar[planeIndex].yawAvgCount;
        //}

        e1 = BoundaryDiff(selfPlane->_roll, overloadVar[planeIndex].oldRoll);
        //if (selfPlane->_roll != overloadVar[planeIndex].oldRoll)
        //{
        //  overloadVar[planeIndex].rollAvgCount++;
        //  overloadVar[planeIndex].rollDiffSum += e1;
        //  avgRoll = overloadVar[planeIndex].rollDiffSum / overloadVar[planeIndex].rollAvgCount;
        //}

        f1 = BoundaryDiff(selfPlane->_pitch, overloadVar[planeIndex].oldPitch);
        //if (selfPlane->_pitch != overloadVar[planeIndex].oldPitch)
        //{
        //  overloadVar[planeIndex].pitchAvgCount++;
        //  overloadVar[planeIndex].pitchDiffSum += f1;
        //  avgPitch = overloadVar[planeIndex].pitchDiffSum / overloadVar[planeIndex].pitchAvgCount;
        //}

        double num = ((a1 + b1 + c1) + (d1 + e1 + f1) + 9.8) / 9.8;
        //double num = ((a1 + b1 + c1) + (d1 / avgYaw + e1 / avgRoll + f1 / avgPitch) + 9.8) / 9.8;
        selfPlane->_overload = std::round(num * 10) / 10.0;

        overloadVar[planeIndex].old_velocity_north = selfPlane->_velocity_north;
        overloadVar[planeIndex].old_velocity_east = selfPlane->_velocity_east;
        overloadVar[planeIndex].old_velocity_downward = selfPlane->_velocity_downward;
        overloadVar[planeIndex].oldYaw = selfPlane->_yaw;
        overloadVar[planeIndex].oldRoll = selfPlane->_roll;
        overloadVar[planeIndex].oldPitch = selfPlane->_pitch;

        overloadVar[planeIndex].stepCount = 0;
    }
}

//执行雷达扫描
void execRadarScan(const int index)
{
    const Aircraft_Object_C* tarPlane = nullptr;

    for (int i = 0; i < planeCount; i++)
    {
        if (index == i || battlefield->aircraft_list[i].base_team == planeStates[index]._team)
            continue;

        tarPlane = &(battlefield->aircraft_list[i]);

        if (planeStates[index]._radarMode == RadarMode_E::HGSingle)//&& tarPlane->radar_state == RadarState_E::Close )
            continue;

        double lonf = tarPlane->coordinate_longitude - planeStates[index]._longitude;
        double latf = tarPlane->coordinate_latitude - planeStates[index]._latitude;
        double altf = tarPlane->coordinate_altitude - planeStates[index]._altitude;

        if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude,
            planeStates[index]._longitude, planeStates[index]._latitude) <= planeStates[index]._radar_range)
        {
            double yawRight = planeStates[index]._radar_azimuth + planeStates[index]._radar_hBeamWidth / 2;
            double yawLeft = planeStates[index]._radar_azimuth - planeStates[index]._radar_hBeamWidth / 2;

            Eigen::Vector3d location = locationTarget(&(planeStates[index]), getPlaneStateByID(tarPlane->Sim_id));
            double z360 = Covert180To360(location.z());
            if (z360 <= yawRight && z360 >= yawLeft)
            {
                double pitchTop = planeStates[index]._radar_elevation + planeStates[index]._radar_vBeamWidth / 2;
                double pitchBottom = planeStates[index]._radar_elevation - planeStates[index]._radar_vBeamWidth / 2;
                if (location.y() <= pitchTop && location.y() >= pitchBottom)
                {
                    bool isExist = false;
                    for (int ii = 0; ii < Max_Plane_Count; ii++)
                    {
                        if (planeStates[index]._raderCapturePlanesID[ii] == tarPlane->Sim_id)
                        {
                            isExist = true;
                            break;
                        }
                    }
                    if (!isExist)
                    {
                        if (planeStates[index]._raderCaptureCount + 1 < Max_Plane_Count)
                        {
                            planeStates[index]._raderCapturePlanesID[planeStates[index]._raderCaptureCount] = tarPlane->Sim_id;
                            planeStates[index]._raderCaptureCount++;
                        }
                    }

                    PlaneState_S* pTemp = getPlaneStateByID(tarPlane->Sim_id);
                    if (pTemp == nullptr)
                        return;

                    int rwsindex = -1, zeroIndex = -1;
                    for (int rwsi = 0; rwsi < Max_Warn_Count; rwsi++)
                    {
                        if (pTemp->_recvRadarWarnState[rwsi]._targetID == planeStates[index]._planeID)
                        {
                            rwsindex = rwsi;
                        }
                        else if (zeroIndex == -1 && pTemp->_recvRadarWarnState[rwsi]._targetID == 0)
                        {
                            zeroIndex = rwsi;
                        }
                    }

                    if (zeroIndex == -1)
                        break;

                    if (rwsindex == -1)
                    {
                        rwsindex = zeroIndex;
                        pTemp->_recvRadarWarnCount++;
                    }

                    pTemp->_recvRadarWarnState[rwsindex]._targetID = planeStates[index]._planeID;
                    pTemp->_recvRadarWarnState[rwsindex]._tarLongitude = planeStates[index]._longitude;
                    pTemp->_recvRadarWarnState[rwsindex]._tarLatitude = planeStates[index]._latitude;
                    pTemp->_recvRadarWarnState[rwsindex]._tarAltitude = planeStates[index]._altitude;
                    pTemp->_recvRadarWarnState[rwsindex]._time = battlefield->time;

                }
            }
        }
    }
}

void execRadarScan_1(const int index)
{
    bool isExist = true;
    const Aircraft_Object_C* tarPlane = nullptr;

    for (int i = 0; i < planeStates[index]._raderCaptureCount; i++)
    {
        isExist = true;
        tarPlane = getAircraftObjectByID(planeStates[index]._raderCapturePlanesID[i]);

        double lonf = tarPlane->coordinate_longitude - planeStates[index]._longitude;
        double latf = tarPlane->coordinate_latitude - planeStates[index]._latitude;
        double altf = tarPlane->coordinate_altitude - planeStates[index]._altitude;

        if (tarPlane->base_team == planeStates[index]._team)
            isExist = false;
        else if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude,
            planeStates[index]._longitude, planeStates[index]._latitude) <= planeStates[index]._radar_range)
        {
            //double yawRight = planeStates[index]._radar_azimuth + planeStates[index]._radar_hBeamWidth / 2;
            //double yawLeft  = planeStates[index]._radar_azimuth - planeStates[index]._radar_hBeamWidth / 2;
            double yawRight = planeStates[index]._radar_all_hBeamWidth / 2;
            double yawLeft = -(planeStates[index]._radar_all_hBeamWidth / 2);

            Eigen::Vector3d location = locationTarget(&(planeStates[index]), getPlaneStateByID(tarPlane->Sim_id));
            //double z360 = Covert180To360(location.z());
            double z360 = location.z();
            if (z360 <= yawRight && z360 >= yawLeft)
            {
                double pitchTop = planeStates[index]._pitch + planeStates[index]._radar_all_vBeamWidth / 2;
                double pitchBottom = planeStates[index]._pitch - planeStates[index]._radar_all_vBeamWidth / 2;
                if (location.y() > pitchTop && location.y() < pitchBottom)
                    isExist = false;
            }
            else
                isExist = false;
        }
        else
            isExist = false;

        if (!isExist)
        {
            planeStates[index]._raderCapturePlanesID[i] = 0;
            planeStates[index]._raderCaptureCount--;

            PlaneState_S* pTemp = getPlaneStateByID(tarPlane->Sim_id);
            if (pTemp == nullptr)
                return;

            for (int rwsi = 0; rwsi < Max_Warn_Count; rwsi++)
            {
                if (pTemp->_recvRadarWarnState[rwsi]._targetID == planeStates[index]._planeID)
                {
                    pTemp->_recvRadarWarnState[rwsi]._targetID = 0;
                    pTemp->_recvRadarWarnCount--;
                }
            }

            memcpy(tempRaderCapturePlanesID, planeStates[index]._raderCapturePlanesID, Max_Plane_Count * sizeof(int));
            memset(planeStates[index]._raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));

            for (int ii = 0, jj = 0; ii < Max_Plane_Count; ii++)
                if (tempRaderCapturePlanesID[ii] != 0)
                    planeStates[index]._raderCapturePlanesID[jj++] = tempRaderCapturePlanesID[ii];
        }
    }
}

void execHGSynergyRadarScan(const int index)
{
    if (planeStates[index]._friendCount < 1)
        return;

#define HWGL 80
    int radarScanCount = 0;
    int radarLength = 0;
    int hAngle = 0;
    int vAngle = 0;
    int rnum = 0;
    const Aircraft_Object_C* tarPlane = nullptr;
    planeStates[index]._raderCaptureCount = 0;

    while (radarScanCount < 2)
    {
        for (int i = 0; i < planeCount; i++)
        {
            tarPlane = &(battlefield->aircraft_list[i]);

            if (index == i || tarPlane->base_live == 0 || tarPlane->base_team == planeStates[index]._team) // || tarPlane->radar_state == RadarState_E::Close )
                continue;

            double lonf;
            double latf;
            double altf;

            if (radarScanCount == 0)
            {
                lonf = planeStates[index]._longitude;
                latf = planeStates[index]._latitude;
                altf = planeStates[index]._altitude;
                radarLength = planeStates[index]._radar_range;
                hAngle = planeStates[index]._radar_hBeamWidth;
                vAngle = planeStates[index]._radar_vBeamWidth;
                rnum = rand() % 100 + 1;
            }
            else
            {
                lonf = planeStates[index]._HGSynergy._originX;
                latf = planeStates[index]._HGSynergy._originY;
                altf = planeStates[index]._HGSynergy._originZ;
                radarLength = planeStates[index]._HGSynergy._overlapLength;
                hAngle = planeStates[index]._HGSynergy._overlapHAngle;
                vAngle = planeStates[index]._HGSynergy._overlapVAngle;
                rnum = -1;
            }

            if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, lonf, latf) <= radarLength)
            {
                double yawRight = planeStates[index]._radar_azimuth + hAngle / 2;
                double yawLeft = planeStates[index]._radar_azimuth - hAngle / 2;

                Eigen::Vector3d location = locationTarget3(
                    planeStates[index]._yaw, planeStates[index]._pitch, planeStates[index]._roll,
                    lonf, latf, altf,
                    tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, tarPlane->coordinate_altitude);

                if (location.z() <= yawRight && location.z() >= yawLeft)
                {
                    double pitchTop = planeStates[index]._radar_elevation + vAngle / 2;
                    double pitchBottom = planeStates[index]._radar_elevation - vAngle / 2;

                    if (location.y() <= pitchTop && location.y() >= pitchBottom)
                    {
                        if (radarScanCount == 1 || (radarScanCount == 0 && (rnum >= 0 && rnum <= HWGL)))
                        {
                            bool isExist = false;
                            for (int rci = 0; rci < planeStates[index]._raderCaptureCount; rci++)
                            {
                                if (planeStates[index]._raderCapturePlanesID[rci] == tarPlane->Sim_id)
                                {
                                    isExist = true;
                                    break;
                                }
                            }
                            if (!isExist)
                            {
                                planeStates[index]._raderCapturePlanesID[planeStates[index]._raderCaptureCount] = tarPlane->Sim_id;
                                planeStates[index]._raderCaptureCount++;

                                PlaneState_S* pTemp = getPlaneStateByID(tarPlane->Sim_id);
                                if (pTemp == nullptr)
                                    return;
                                if (pTemp->_recvRadarWarnCount == Max_Warn_Count)
                                    pTemp->_recvRadarWarnCount = 0;

                                pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._targetID = planeStates[index]._planeID;
                                pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarLongitude = planeStates[index]._longitude;
                                pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarLatitude = planeStates[index]._latitude;
                                pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarAltitude = planeStates[index]._altitude;
                                pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._time = battlefield->time;
                                pTemp->_recvRadarWarnCount++;
                            }
                        }
                    }
                }
            }
        }
        radarScanCount++;
    }
}

void execHGSynergyRadarScan1(const int index)
{
    if (planeStates[index]._friendCount < 1)
        return;

    int radarLength = 0;
    int hAngle = 0;
    int vAngle = 0;
    int rnum = 0;
    const Aircraft_Object_C* tarPlane = nullptr;

    for (int i = 0; i < planeCount; i++)
    {
        tarPlane = &(battlefield->aircraft_list[i]);

        if (planeStates[index]._radarMode != RadarMode_E::HGSynergy || index == i || tarPlane->base_live == 0 || tarPlane->base_team == planeStates[index]._team) // || tarPlane->radar_state == RadarState_E::Close )
            continue;

        double lonf;
        double latf;
        double altf;

        lonf = planeStates[index]._HGSynergy._originX;
        latf = planeStates[index]._HGSynergy._originY;
        altf = planeStates[index]._HGSynergy._originZ;
        radarLength = planeStates[index]._HGSynergy._overlapLength;
        hAngle = planeStates[index]._HGSynergy._overlapHAngle;
        vAngle = planeStates[index]._HGSynergy._overlapVAngle;
        rnum = -1;

        if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, lonf, latf) <= radarLength)
        {
            double yawRight = planeStates[index]._radar_azimuth + hAngle / 2;
            double yawLeft = planeStates[index]._radar_azimuth - hAngle / 2;

            Eigen::Vector3d location = locationTarget3(
                planeStates[index]._yaw, planeStates[index]._pitch, planeStates[index]._roll,
                lonf, latf, altf,
                tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, tarPlane->coordinate_altitude);

            if (location.z() <= yawRight && location.z() >= yawLeft)
            {
                double pitchTop = planeStates[index]._radar_elevation + vAngle / 2;
                double pitchBottom = planeStates[index]._radar_elevation - vAngle / 2;

                if (location.y() <= pitchTop && location.y() >= pitchBottom)
                {
                    bool isExist = false;
                    for (int rci = 0; rci < planeStates[index]._raderCaptureCount; rci++)
                    {
                        if (planeStates[index]._raderCapturePlanesID[rci] == tarPlane->Sim_id)
                        {
                            isExist = true;
                            break;
                        }
                    }
                    if (!isExist)
                    {
                        planeStates[index]._raderCapturePlanesID[planeStates[index]._raderCaptureCount] = tarPlane->Sim_id;
                        planeStates[index]._raderCaptureCount++;

                        PlaneState_S* pTemp = getPlaneStateByID(tarPlane->Sim_id);
                        if (pTemp == nullptr)
                            return;
                        if (pTemp->_recvRadarWarnCount == Max_Warn_Count)
                            pTemp->_recvRadarWarnCount = 0;

                        pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._targetID = planeStates[index]._planeID;
                        pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarLongitude = planeStates[index]._longitude;
                        pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarLatitude = planeStates[index]._latitude;
                        pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._tarAltitude = planeStates[index]._altitude;
                        pTemp->_recvRadarWarnState[pTemp->_recvRadarWarnCount]._time = battlefield->time;
                        pTemp->_recvRadarWarnCount++;
                    }
                }
            }
        }
    }
}

void execHGSynergyRadarScan1_1(const int index)
{
    if (planeStates[index]._friendCount < 1)
        return;

    int radarLength = 0;
    int hAngle = 0;
    int vAngle = 0;
    const Aircraft_Object_C* tarPlane = nullptr;
    bool isExist = true;

    for (int i = 0; i < planeStates[index]._raderCaptureCount; i++)
    {
        tarPlane = getAircraftObjectByID(planeStates[index]._raderCapturePlanesID[i]);

        if (planeStates[index]._radarMode != RadarMode_E::HGSynergy || tarPlane->base_live == 0)
            continue;

        double lonf;
        double latf;
        double altf;

        isExist = true;
        lonf = planeStates[index]._HGSynergy._originX;
        latf = planeStates[index]._HGSynergy._originY;
        altf = planeStates[index]._HGSynergy._originZ;
        radarLength = planeStates[index]._HGSynergy._overlapLength;
        hAngle = planeStates[index]._HGSynergy._overlapHAngle;
        vAngle = planeStates[index]._HGSynergy._overlapVAngle;

        if (GetDistance(tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, lonf, latf) <= radarLength)
        {
            double yawRight = planeStates[index]._radar_azimuth + hAngle / 2;
            double yawLeft = planeStates[index]._radar_azimuth - hAngle / 2;

            Eigen::Vector3d location = locationTarget3(
                planeStates[index]._yaw, planeStates[index]._pitch, planeStates[index]._roll,
                lonf, latf, altf,
                tarPlane->coordinate_longitude, tarPlane->coordinate_latitude, tarPlane->coordinate_altitude);

            if (location.z() <= yawRight && location.z() >= yawLeft)
            {
                double pitchTop = planeStates[index]._radar_elevation + vAngle / 2;
                double pitchBottom = planeStates[index]._radar_elevation - vAngle / 2;

                if (location.y() > pitchTop && location.y() < pitchBottom)
                    isExist = false;
            }
            else
                isExist = false;
        }
        else
            isExist = false;

        if (!isExist)
        {
            planeStates[index]._raderCapturePlanesID[i] = 0;
            planeStates[index]._raderCaptureCount--;

            PlaneState_S* pTemp = getPlaneStateByID(tarPlane->Sim_id);
            if (pTemp == nullptr)
                return;

            for (int rwsi = 0; rwsi < Max_Warn_Count; rwsi++)
            {
                if (pTemp->_recvRadarWarnState[rwsi]._targetID == planeStates[index]._planeID)
                {
                    pTemp->_recvRadarWarnState[rwsi]._targetID = 0;
                    pTemp->_recvRadarWarnCount--;
                }
            }

            memcpy(tempRaderCapturePlanesID, planeStates[index]._raderCapturePlanesID, Max_Plane_Count * sizeof(int));
            memset(planeStates[index]._raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));

            for (int ii = 0, jj = 0; ii < Max_Plane_Count; ii++)
                if (tempRaderCapturePlanesID[ii] != 0)
                    planeStates[index]._raderCapturePlanesID[jj++] = tempRaderCapturePlanesID[ii];
        }
    }
}

void lostCaptureTarget(PlaneState_S* self, int targetID)
{
    if (self == nullptr)
        return;

    if (targetID == -1)
        targetID = self->_targetID;

    PlaneState_S* target = getPlaneStateByID(targetID);

    if (target == nullptr)
        return;

    for (int i = 0; i < self->_raderCaptureCount; i++)
    {
        if (self->_raderCapturePlanesID[i] == targetID)
        {
            self->_raderCapturePlanesID[i] = 0;
            self->_raderCaptureCount--;

            for (int rwsi = 0; rwsi < Max_Warn_Count; rwsi++)
            {
                if (target->_recvRadarWarnState[rwsi]._targetID == self->_planeID)
                {
                    target->_recvRadarWarnState[rwsi]._targetID = 0;
                    target->_recvRadarWarnCount--;
                }
            }

            memcpy(tempRaderCapturePlanesID, self->_raderCapturePlanesID, Max_Plane_Count * sizeof(int));
            memset(self->_raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));

            for (int ii = 0, jj = 0; ii < Max_Plane_Count; ii++)
                if (tempRaderCapturePlanesID[ii] != 0)
                    self->_raderCapturePlanesID[jj++] = tempRaderCapturePlanesID[ii];
        }
    }
}

//判断某架飞机雷达是否扫描到敌机
bool isRaderCapture(const int in_SrcID, const int in_TarID)
{
    PlaneState_S* srcPlane = getPlaneStateByID(in_SrcID);
    PlaneState_S* tarPlane = getPlaneStateByID(in_TarID);

    if (srcPlane == nullptr || tarPlane == nullptr)
        return false;

    double lonf = tarPlane->_longitude - srcPlane->_longitude;
    double latf = tarPlane->_latitude - srcPlane->_latitude;
    double altf = tarPlane->_altitude - srcPlane->_altitude;

    if (lonf == 0 && latf == 0)
    {
        return true;
    }
    if (GetDistance(tarPlane->_longitude, tarPlane->_latitude,
        srcPlane->_longitude, srcPlane->_latitude) <= srcPlane->_radar_range)
    {
        double yawRight = srcPlane->_radar_azimuth + srcPlane->_radar_hBeamWidth / 2;
        double yawLeft = srcPlane->_radar_azimuth - srcPlane->_radar_hBeamWidth / 2;

        Eigen::Vector3d location = locationTarget(srcPlane, getPlaneStateByID(tarPlane->_planeID));

        if (location.z() <= yawRight && location.z() >= yawLeft)
        {
            double pitchTop = srcPlane->_radar_elevation + srcPlane->_radar_vBeamWidth / 2;
            double pitchBottom = srcPlane->_radar_elevation - srcPlane->_radar_vBeamWidth / 2;
            if (location.y() <= pitchTop && location.y() >= pitchBottom)
                return true;
        }
    }
    return false;
}

//刷新飞机状态
inline void setPlaneState(const int i, const PlaneState_S* planePtr)
{
    planeStates[i]._isRadarModeChange = planePtr->_isRadarModeChange;
    planeStates[i]._isSpeedAdjust = planePtr->_isSpeedAdjust;
    planeStates[i]._equipmentType = planePtr->_equipmentType;
    planeStates[i]._battleTime = battlefield->time;
    planeStates[i]._planeID = battlefield->aircraft_list[i].Sim_id;
    planeStates[i]._team = battlefield->aircraft_list[i].base_team;
    planeStates[i]._longitude = battlefield->aircraft_list[i].coordinate_longitude;
    planeStates[i]._latitude = battlefield->aircraft_list[i].coordinate_latitude;
    planeStates[i]._altitude = battlefield->aircraft_list[i].coordinate_altitude;
    planeStates[i]._roll = battlefield->aircraft_list[i].coordinate_roll;
    planeStates[i]._pitch = battlefield->aircraft_list[i].coordinate_pitch;
    planeStates[i]._yaw = battlefield->aircraft_list[i].coordinate_yaw;
    planeStates[i]._velocity_north = battlefield->aircraft_list[i].velocity_north;
    planeStates[i]._velocity_east = battlefield->aircraft_list[i].velocity_east;
    planeStates[i]._velocity_downward = battlefield->aircraft_list[i].velocity_downward;
    planeStates[i].TAS = sqrt(pow(battlefield->aircraft_list[i].velocity_east, 2) +
        pow(battlefield->aircraft_list[i].velocity_north, 2) +
        pow(battlefield->aircraft_list[i].velocity_downward, 2));
    planeStates[i]._radarState = battlefield->aircraft_list[i].radar_state;
    planeStates[i]._radar_azimuth = battlefield->aircraft_list[i]._radar_azimuth;
    planeStates[i]._radar_elevation = battlefield->aircraft_list[i]._radar_elevation;
    planeStates[i]._radar_range = planePtr->_radar_range;
    planeStates[i]._radar_view_range = planePtr->_radar_view_range;
    planeStates[i]._radar_hBeamWidth = planePtr->_radar_hBeamWidth;
    planeStates[i]._radar_vBeamWidth = planePtr->_radar_vBeamWidth;
    planeStates[i]._radar_vBeamWidth_upper = planePtr->_radar_vBeamWidth_upper;
    planeStates[i]._radar_vBeamWidth_below = planePtr->_radar_vBeamWidth_below;
    planeStates[i]._radar_all_hBeamWidth = planePtr->_radar_all_hBeamWidth;
    planeStates[i]._radar_all_vBeamWidth = planePtr->_radar_all_vBeamWidth;
    planeStates[i]._radar_all_range = planePtr->_radar_all_range;
    planeStates[i]._radar_all_azimuth = planePtr->_radar_all_azimuth;
    planeStates[i]._radar_all_elevation = planePtr->_radar_all_elevation;
    planeStates[i]._radarMode = planePtr->_radarMode;

    planeStates[i]._HGSynergy = planePtr->_HGSynergy;
    planeStates[i]._ESMSynergy = planePtr->_ESMSynergy;
    planeStates[i]._relayGuidance = planePtr->_relayGuidance;

    planeStates[i]._roll_ctrl = planePtr->_roll_ctrl;
    planeStates[i]._pitch_ctrl = planePtr->_pitch_ctrl;
    planeStates[i]._yaw_ctrl = planePtr->_yaw_ctrl;
    planeStates[i]._throttle = planePtr->_throttle;

    planeStates[i]._targetID = planePtr->_targetID;
    if (planePtr->_isAlive != -2)
    {
        planeStates[i]._isAlive = battlefield->aircraft_list[i].base_live;
    }
    planeStates[i]._missileCount = planePtr->_missileCount;
    planeStates[i]._isShoot = planePtr->_isShoot;
    planeStates[i]._limitIndex = planePtr->_limitIndex;
    planeStates[i]._isWaypointMode = planePtr->_isWaypointMode;
    planeStates[i]._overload = planePtr->_overload;

    memcpy(planeStates[i]._missileState, planePtr->_missileState, sizeof(MissileState_S) * Max_Missile_Count);

    ////清理接收雷达波警告
    //for ( int pIndex = 0; pIndex < planeCount; ++pIndex )
    //{
    //  for ( int j = 0 ; j < Max_Warn_Count ; ++j )
    //  {
    //      if ( (battlefield->time - planeStates[pIndex]._recvRadarWarnState[j]._time) > 0.2 )
    //      {
    //          memset(planeStates[pIndex]._recvRadarWarnState + j, 0, sizeof(WarnState_S));
    //      }
    //  }
    //}

    if (planeStates[i]._radarState == RadarState_E::Scan)
    {
        if (planeStates[i]._radarMode == RadarMode_E::HGSynergy)
        {
            execHGSynergyRadarScan1(i);
            execHGSynergyRadarScan1_1(i);
        }
        else
        {
            execRadarScan(i);
            execRadarScan_1(i);
        }
    }
#if SEA_AIR_ENABLE_WGUA
    if (int(battlefield->time * 10) % 10 == 0)
        SetWGData(planeStates + i, 1);
#endif
}

//设置飞机编组信息
void setPlaneTeam()
{
    for (int i = 0; i < planeCount; i++)
    {
        for (int j = 0; j < planeCount; j++)
        {
            if (planeStates[i]._friendCount < Max_Plane_Count &&
                planeStates[i]._planeID != planeStates[j]._planeID &&
                planeStates[i]._team == planeStates[j]._team)
            {
                planeStates[i]._friendList[planeStates[i]._friendCount++] = planeStates[j]._planeID;
            }
        }
    }
}

//初始化环境
bool InitEnv(const char* acmiPath, PlaneState_S* planes, uint32_t size)
{
    StartJoystick();

    if (size == 0)
        return false;

    srand((unsigned int)time(NULL));
    radarDataMap.clear();
    memset(overloadVar, 0, sizeof(OverloadVar));

    if (battlefield != nullptr)
    {
        delete battlefield;
        battlefield = nullptr;
    }
    if (battlefield == nullptr)
    {
        battlefield = new Battlefield_C;
        battlefield->aircraft_count = planeCount = size;
        battlefield->time = 0;
        memset(planeStates, 0, sizeof(PlaneState_S) * Max_Plane_Count);
        missStepTime = 0;
    }

    if (tacview_show != nullptr)
    {
#if SEA_AIR_ENABLE_TACVIEW_SERVER
        if (tacview_show->server != nullptr && tacview_show->server->isOpen())
            tacview_show->server->Close();
#endif
        delete tacview_show;
        tacview_show = nullptr;
    }
    if (tacview_show == nullptr)
    {
        tacview_show = new TacViewOutput;
        tacview_show->state->time = 0;
#if SEA_AIR_ENABLE_TACVIEW_SERVER
        tacview_show->InitServer();
#endif
#if SEA_AIR_ENABLE_WGUA
        if (!InitWG_S())
            cout << "InitWG_S init error";
#endif
    }

    if (acmiFile != nullptr)
    {
        if (acmiFile->isOpen())
            acmiFile->Close();
        delete acmiFile;
        acmiFile = nullptr;
    }
    if (acmiFile == nullptr)
    {
#if SEA_AIR_ENABLE_ACMI_FILE
        acmiFile = new TacViewFile_T;
        Header_T header;
        memset(&header, 0, sizeof(header));
        strncpy(header.reference_time, "2024-01-01T00:00:00Z", max_str);
        strncpy(header.recording_time, "2024-01-01T00:00:00Z", max_str);
        if (acmiPath != nullptr)
            acmiFile->Open(acmiPath, header);
#endif
    }

    battlefield->InitCoordinate(planes[0]._longitude, planes[0]._latitude, planes[0]._altitude);
    //battlefield->InitCoordinate(0, 0, 0);

    for (uint32_t i = 0; i < size; i++)
    {
        planes[i]._isAlive = true;
        planes[i]._isShoot = false;
        planes[i]._isWaypointMode = false;
        planes[i]._missileCount = Max_Missile_Count;
        planes[i]._recvRadarWarnCount = 0;

        battlefield->aircraft_list[i].Init(planes[i]._planeID, planes[i]._equipmentType, planes[i]._team,
            planes[i]._longitude, planes[i]._latitude, planes[i]._altitude,
            planes[i]._roll, planes[i]._pitch, planes[i]._yaw, 0, 0, 0);

        battlefield->aircraft_list[i].craft_handle << planes[i]._roll_ctrl, planes[i]._pitch_ctrl, planes[i]._yaw_ctrl, planes[i]._throttle;

        string color, pilot;
        if (planes[i]._team == 1) {
            color = "Red";
            //pilot = "Human";
        }
        else if (planes[i]._team == 2) {
            color = "Blue";
            //pilot = "AI";
        }
        else if (planes[i]._team == 3) {
            color = "Yellow";
        }
        //pilot = std::to_string(planes[i]._planeID);
        if (strlen(planes[i]._pilot) > 0)
            pilot = planes[i]._pilot;
        else
            pilot = "";

        tacview_show->InitOneObject(battlefield->time, i, battlefield->aircraft_list[i].Sim_id, battlefield->aircraft_list[i].base_name,
            battlefield->aircraft_list[i].base_type, pilot, color, 15.5, 9.7, 4.8,
            battlefield->aircraft_list[i].coordinate_longitude, battlefield->aircraft_list[i].coordinate_latitude, battlefield->aircraft_list[i].coordinate_altitude,
            battlefield->aircraft_list[i].coordinate_roll, battlefield->aircraft_list[i].coordinate_pitch, battlefield->aircraft_list[i].coordinate_yaw,
            planes[i]._radarState, true, planes[i]._radar_azimuth, planes[i]._radar_elevation,
            planes[i]._radar_view_range, planes[i]._radar_hBeamWidth, planes[i]._radar_vBeamWidth, planes[i]._radar_all_hBeamWidth, planes[i]._radar_all_vBeamWidth, planes[i]._radar_all_range, planes[i]._radar_all_azimuth, planes[i]._radar_all_elevation);

        radarDataMap[planes[i]._planeID].isLeft = true;
        radarDataMap[planes[i]._planeID].Azimuth = 0;

        setPlaneState(i, &(planes[i]));
    }
    setPlaneTeam();

    if (acmiFile != nullptr && acmiFile->isOpen())
        acmiFile->Step(*(tacview_show->state));

#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendOneFrame(battlefield->time);
#endif

    return true;
}

int AddCircle(double longitude, double latitude, double altitude, int radius, Color_E color)
{
    static int circleId = 25000;

    CircleData circleData;
    circleData.cid = circleId++;
    circleData.longitude = longitude;
    circleData.latitude = latitude;
    circleData.altitude = altitude;
    circleData.radius = radius;
    circleData.color = color;

    tacview_show->OneFrameAddCircle(circleData);

    //if ( acmiFile->isOpen() )
        //acmiFile->AddCircle(*(tacview_show->state));

#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendAddCircleFrame(battlefield->time);
#endif

    return circleData.cid;
}

void RemoveCircle(int cid)
{
    CircleData circleData;
    circleData.cid = cid;

    tacview_show->OneFrameRemoveCircle(circleData);

    //if ( acmiFile->isOpen() )
        //acmiFile->RemoveCircle(*(tacview_show->state));

#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendAddCircleFrame(battlefield->time);
#endif
}

void AddBoundary(int num, double baseLongitude, double baseLatitude, double baseAltitude, double xLen, double yLen, double zLen, Color_E color)
{
    tacview_show->OneFrameBoundaryState(num, baseLongitude, baseLatitude, baseAltitude, xLen, yLen, zLen, (int)color);
    if (acmiFile != nullptr && acmiFile->isOpen())
        acmiFile->AddBoundary(*(tacview_show->state));
#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendBoundaryFrame(battlefield->time);
#endif
}

void DrawCrossGrid(double baseLongitude, double baseLatitude, double baseAltitude, int childSize, int hNum, int vNum, Color_E color)
{
    if (hNum > 10 && hNum < 1)
        return;
    if (vNum > 10 && vNum < 1)
        return;

    CrossGrid crossGrid;
    crossGrid.baseLongitude = baseLongitude;
    crossGrid.baseLatitude = baseLatitude;
    crossGrid.baseAltitude = baseAltitude;
    crossGrid.childSize = childSize;
    crossGrid.hNum = hNum;
    crossGrid.vNum = vNum;
    crossGrid.color = color;

    tacview_show->OneFrameDrawCrossGrid(crossGrid);

    //if ( acmiFile->isOpen() )
    //  acmiFile->AddBoundary(*(tacview_show->state));

#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendDrawCrossGridFrame(battlefield->time);
#endif
}

static time_t beginSecond = 0;
void RadomMoveCrossGrid(PlaneState_S* planes, int second)
{
#if SEA_AIR_ENABLE_TACVIEW_SERVER
    extern int centerPointMapIndex;
    extern std::map<int, std::pair<double, double>> centerPointMap;

    if (beginSecond == 0)
        beginSecond = time(0);

    if (time(0) - beginSecond >= second)
    {
        int r = rand() % centerPointMapIndex;
        Aircraft_Object_C* aaa = getAircraftObjectByID(planes->_planeID);
        aaa->coordinate_longitude = planes->_longitude = centerPointMap[r].first;
        aaa->coordinate_latitude = planes->_latitude = centerPointMap[r].second;
        beginSecond = time(0);
    }
#else
    (void)planes;
    (void)second;
#endif
}

void SetPathPlan(const PathTomonitor_S& pathTomonitor)
{
    tacview_show->OneFramePathPlanState(pathTomonitor);
    if (acmiFile != nullptr && acmiFile->isOpen())
        acmiFile->AddPathPlan(*(tacview_show->state));
#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendPathPlanFrame(battlefield->time);
#endif
}

//飞行器单步解算
bool EnvStep(double dt, PlaneState_S* planes)
{
#if SEA_AIR_ENABLE_WGUA
    static std::uint64_t startTime = 0;
    int btnCode = OutputJoystick(planes);
    if ((sea_air_tick_count64() - startTime) > 200 && btnCode == 8)
    {
        //cout << btnCode << "|" << SetData(&btnCode, sizeof(int)) << endl;
        SetData(&btnCode, sizeof(int));
        startTime = sea_air_tick_count64();
    }
#else
    OutputJoystick(planes);
#endif

    battlefield->time += dt;

    for (int i = 0; i < battlefield->aircraft_count; i++)
    {
        if (planes[i]._radarState != planeStates[i]._radarState || planes[i]._radarMode != planeStates[i]._radarMode)
            planes[i]._isRadarModeChange = true;

        if (planes[i]._isWaypointMode)
        {
            battlefield->aircraft_list[i].craft_handle(0) = planes[i]._roll_ctrl;
            battlefield->aircraft_list[i].craft_handle(1) = planes[i]._pitch_ctrl;
            battlefield->aircraft_list[i].craft_handle(2) = planes[i]._yaw_ctrl;
        }
        else
        {
            if (planes[i]._roll > battlefield->aircraft_list[i].coordinate_roll)
                battlefield->aircraft_list[i].craft_handle(0) = planes[i]._roll_ctrl;
            else if (planes[i]._roll < battlefield->aircraft_list[i].coordinate_roll)
                battlefield->aircraft_list[i].craft_handle(0) = -planes[i]._roll_ctrl;
            else
                battlefield->aircraft_list[i].craft_handle(0) = 0;

            if (planes[i]._limitIndex != -1 &&
                UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->altitude != -1.0 &&
                UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->altitude * (1 - HEIGHT_LIMIT_RATIO) < planes[i]._altitude)
            {
                battlefield->aircraft_list[i].craft_handle(1) = 0;
            }
            else
            {
                if (planes[i]._pitch > battlefield->aircraft_list[i].coordinate_pitch)
                    battlefield->aircraft_list[i].craft_handle(1) = planes[i]._pitch_ctrl;
                else if (planes[i]._pitch < battlefield->aircraft_list[i].coordinate_pitch)
                    battlefield->aircraft_list[i].craft_handle(1) = -planes[i]._pitch_ctrl;
                else
                    battlefield->aircraft_list[i].craft_handle(1) = 0;
            }

            if (fabs(planes[i]._yaw - battlefield->aircraft_list[i].coordinate_yaw) < 0.05)
                battlefield->aircraft_list[i].craft_handle(2) = 0;
            else
            {
                if (getYawPoint(battlefield->aircraft_list[i].coordinate_yaw, planes[i]._yaw) > 0)
                    battlefield->aircraft_list[i].craft_handle(2) = planes[i]._yaw_ctrl;
                else
                    battlefield->aircraft_list[i].craft_handle(2) = -planes[i]._yaw_ctrl;
            }

            if (planes[i]._limitIndex != -1 &&
                UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->speed != -1.0 &&
                planeStates[i].TAS < 340.0 * 2.5)
            {
                if (planes[i].TAS > UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->speed * (1 - SPEED_LIMIT_RATIO))
                {
                    if (!planes[i]._isSpeedAdjust)
                        planes[i]._isSpeedAdjust = true;
                    planes[i]._throttle -= planes[i]._throttle * 0.1;
                    //if ( i == 1)
                    //{
                    //  cout << UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->speed * (1 - SPEED_LIMIT_RATIO)
                    //      << "|" << planes[i].TAS << "|" << planes[i]._velocity_east << endl;
                    //}
                }
                else if (planes[i]._isSpeedAdjust && planes[i].TAS < UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->speed * (1 - SPEED_LIMIT_RATIO))
                {
                    planes[i]._throttle += planes[i]._throttle * 0.1;
                }
            }
        }

        battlefield->aircraft_list[i].craft_handle(3) = planes[i]._throttle;
        battlefield->aircraft_list[i].radar_state = planes[i]._radarState;
        battlefield->aircraft_list[i]._radar_azimuth = planes[i]._radar_azimuth;
        if (battlefield->aircraft_list[i]._radar_elevation != planes[i]._radar_elevation)
        {
            if (planes[i]._radar_elevation > planes[i]._radar_vBeamWidth_upper || planes[i]._radar_elevation < planes[i]._radar_vBeamWidth_below)
                planes[i]._radar_elevation = battlefield->aircraft_list[i]._radar_elevation;
            else
                battlefield->aircraft_list[i]._radar_elevation = planes[i]._radar_elevation;
        }

        if (planes[i]._radarMode == RadarMode_E::RM_Normal ||
            planes[i]._radarMode == RadarMode_E::HGSingle ||
            planes[i]._radarMode == RadarMode_E::HGSynergy ||
            planes[i]._radarMode == RadarMode_E::ESMSynergy ||
            planes[i]._radarMode == RadarMode_E::RelayGuidance)
        {
            if (battlefield->aircraft_list[i].radar_state == RadarState_E::Track)
            {
                int targetID = planes[i]._targetID;
                if (planes[i]._ESMSynergy._isOpen && !planes[i]._ESMSynergy._isMaster)
                    targetID = planes[i]._ESMSynergy._enemyId;

                double tarYaw = getLockTargetYaw(i, targetID, planes[i]._radarMode);
                if (tarYaw != LOCKTARGETYAW_NOTFIND)
                {
                    battlefield->aircraft_list[i]._radar_azimuth = tarYaw;
                    if (planes[i]._isShoot && planes[i]._missileCount > 0)
                    {
                        Aircraft_Object_C* objPtr = getAircraftObjectByID(planes[i]._targetID);
                        if (objPtr != nullptr)
                        {
                            if (planes[i]._limitIndex != -1)
                            {
                                battlefield->missile_list[battlefield->missile_count].max_journey = UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->maxJourney;
                                battlefield->missile_list[battlefield->missile_count].terminal_guidance_distance = UNIT_OBJECT_LIMITS[planes[i]._limitIndex]->terminalGuidanceDistance;
                            }

                            if (planes[i]._equipmentType == EquipmentType_E::FRIGATE1)
                                battlefield->MissileFire_SeaSkimming(battlefield->aircraft_list[i], *objPtr);
                            else
                                battlefield->MissileFire(battlefield->aircraft_list[i], *objPtr);

                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._missileID = battlefield->missile_list[battlefield->missile_count - 1].Sim_id;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._missile_live = battlefield->missile_list[battlefield->missile_count - 1].missile_live;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._radar_state = battlefield->missile_list[battlefield->missile_count - 1].radar_state;
                            //planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._radar_state   = battlefield->missile_list[battlefield->missile_count - 1].self_lead;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._targetID = objPtr->Sim_id;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._ruidanceMode = GuidanceMode_E::GM_Normal;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._heightAtLaunch = planes[i]._altitude;
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._distanceAtLaunch = GetDistance(objPtr->coordinate_longitude, objPtr->coordinate_latitude, planes[i]._longitude, planes[i]._latitude);
                            planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]._isHit = IsHitShip(planes[i]._missileState[Max_Missile_Count - planes[i]._missileCount]);
                            planes[i]._missileCount--;
                        }
                    }

                    if (planes[i]._radarMode == RadarMode_E::ESMSynergy)
                    {
                        tacview_show->state->object[i].focused_target_valid = 1;
                        tacview_show->state->object[i].focusedTarget = planes[i]._ESMSynergy._friendId;
                    }
                    else if (planes[i]._radarMode == RadarMode_E::RelayGuidance)
                    {
                        tacview_show->state->object[i].focused_target_valid = 1;
                        tacview_show->state->object[i].focusedTarget = planes[i]._relayGuidance._missileID;
                    }
                }
                else
                {
                    battlefield->aircraft_list[i]._radar_azimuth = getRadarUpdateRadarInfo(&(planes[i]));
                    battlefield->aircraft_list[i].radar_state = planes[i]._radarState = RadarState_E::Scan;
                    planes[i]._radarMode = RadarMode_E::RM_Normal;
                    //planes[i]._targetID                            = 0;
                    tacview_show->state->object[i].focusedTarget = -1;
                }
            }
            else if (planes[i]._radarMode == RadarMode_E::RM_Normal &&
                battlefield->aircraft_list[i].radar_state == RadarState_E::Scan)
            {
                battlefield->aircraft_list[i]._radar_azimuth = getRadarUpdateRadarInfo(&(planes[i]));
                tacview_show->state->object[i].focusedTarget = -1;
            }
        }
        else if (planes[i]._radarMode == RadarMode_E::HGSynergy)
        {
            //setRadarHGSynergyMode(&planes[i]);
            if (!IsHGSynergy(&(planes[i])))
                setRadarNormalMode(&(planes[i]));
            tacview_show->state->object[i].focusedTarget = -1;
        }

        planes[i]._isShoot = false;
        if ((planes[i]._equipmentType == FIGHTER ||
            planes[i]._equipmentType == AWACS_B52 ||
            planes[i]._equipmentType == FIGHTER_J15) &&
            planes[i]._isAlive != -2)
        {
            setOverload(&(planes[i]));
            battlefield->aircraft_list[i].Run(dt);

            if (battlefield->aircraft_list[i].coordinate_altitude < MINIMUM_HEIGHT)
                battlefield->aircraft_list[i].base_live = -1;
        }
        else if ((planes[i]._equipmentType == FRIGATE1 || planes[i]._equipmentType == AIRCRAFTCARRIER_SHANDONG) && planes[i]._isAlive != -2)
        {
            Eigen::Matrix4d craft_state_old = battlefield->aircraft_list[i].craft_state;
            battlefield->aircraft_list[i].Run(dt);
            battlefield->aircraft_list[i].craft_state(0, 2) = craft_state_old(0, 2);
            planes[i]._altitude = battlefield->aircraft_list[i].coordinate_altitude = 0;
        }

        //if ( planes[0]._radarMode == HGSynergy && planes[i]._planeID == 10012 )
        //{
        //  setRadarNormalMode(planes);
        //  setRadarHGSynergyMode1(planes);

        //  battlefield->aircraft_list[i].coordinate_longitude = planes[0]._HGSynergy._originX;
        //  battlefield->aircraft_list[i].coordinate_latitude = planes[0]._HGSynergy._originY;
        //  battlefield->aircraft_list[i].coordinate_altitude = planes[0]._HGSynergy._originZ;
        //  battlefield->aircraft_list[i].coordinate_roll = 0;
        //  battlefield->aircraft_list[i].coordinate_pitch = 0;
        //  battlefield->aircraft_list[i].coordinate_yaw = planes[0]._yaw;
        //  battlefield->aircraft_list[i].radar_state = 1;
        //  battlefield->aircraft_list[i]._radar_azimuth = 0;
        //  planes[i]._radar_range = planes[2]._HGSynergy._overlapLength;
        //  planes[i]._radar_hBeamWidth = planes[0]._HGSynergy._overlapHAngle;
        //  planes[i]._radar_vBeamWidth = planes[0]._HGSynergy._overlapVAngle;

        ///*    tacview_show->OneFrameFlightState(i, battlefield->aircraft_list[i].Sim_id, battlefield->aircraft_list[i].base_live,
        //      battlefield->aircraft_list[i].coordinate_longitude, battlefield->aircraft_list[i].coordinate_latitude, battlefield->aircraft_list[i].coordinate_altitude,
        //      battlefield->aircraft_list[i].coordinate_roll, battlefield->aircraft_list[i].coordinate_pitch, battlefield->aircraft_list[i].coordinate_yaw,
        //      battlefield->aircraft_list[i].radar_state, -1, battlefield->aircraft_list[i]._radar_azimuth, 0, planes[i]._radar_range, planes[i]._radar_hBeamWidth, planes[i]._radar_vBeamWidth);
        //*/
        //  tacview_show->OneFrameFlightState(i, battlefield->aircraft_list[i].Sim_id, battlefield->aircraft_list[i].base_live,
        //  battlefield->aircraft_list[i].coordinate_longitude, battlefield->aircraft_list[i].coordinate_latitude, battlefield->aircraft_list[i].coordinate_altitude,
        //  battlefield->aircraft_list[i].coordinate_roll, battlefield->aircraft_list[i].coordinate_pitch, battlefield->aircraft_list[i].coordinate_yaw,
        //  battlefield->aircraft_list[i].radar_state, -1, battlefield->aircraft_list[i]._radar_azimuth, battlefield->aircraft_list[i]._radar_elevation,
        //  planes[i]._radar_range, planes[i]._radar_hBeamWidth, planes[i]._radar_vBeamWidth, planes[i]._radar_all_hBeamWidth, planes[i]._radar_all_vBeamWidth, planes[i]._radar_all_range, planes[i]._radar_all_azimuth, planes[i]._radar_all_elevation);
        //
        //}
        //else
        //{
        tacview_show->OneFrameFlightState(battlefield->time, i, battlefield->aircraft_list[i].Sim_id, battlefield->aircraft_list[i].base_live,
            battlefield->aircraft_list[i].coordinate_longitude, battlefield->aircraft_list[i].coordinate_latitude, battlefield->aircraft_list[i].coordinate_altitude,
            battlefield->aircraft_list[i].coordinate_roll, battlefield->aircraft_list[i].coordinate_pitch, battlefield->aircraft_list[i].coordinate_yaw,
            battlefield->aircraft_list[i].radar_state, planes[i]._isRadarModeChange, -1, battlefield->aircraft_list[i]._radar_azimuth, battlefield->aircraft_list[i]._radar_elevation,
            planes[i]._radar_view_range, planes[i]._radar_hBeamWidth, planes[i]._radar_vBeamWidth, planes[i]._radar_all_hBeamWidth, planes[i]._radar_all_vBeamWidth, planes[i]._radar_all_range, planes[i]._radar_all_azimuth, planes[i]._radar_all_elevation);

        if (strlen(planes[i]._pilot) > 0)
        {
            tacview_show->state->object[i].base_valid = 1;
            strcpy_s(tacview_show->state->object[i].base_pilot, max_str, planes[i]._pilot);
            strcpy_s(tacview_show->state->object[i].base_label, max_str, planes[i]._pilot);
            tacview_show->state->object[i].isInit = true;
        }

        if (planes[i]._isRadarModeChange)
            planes[i]._isRadarModeChange = false;
        //}

        //导弹飞行计算
        for (int mindex = 0, planesMissileIndex = 0; mindex < battlefield->missile_count && planesMissileIndex < Max_Missile_Count; mindex++)
        {
            //if ( battlefield->missile_list[mindex].missile_live != 1 )
            //  continue;
            if (battlefield->missile_list[mindex].father_id != battlefield->aircraft_list[i].Sim_id)
                continue;

            if (battlefield->missile_list[mindex].p_target_air->base_live == 0)
            {
                if (battlefield->missile_list[mindex].missile_live != -3)
                    battlefield->missile_list[mindex].missile_live = -2;
                battlefield->missile_list[mindex].lead_state = 0;
            }
            else
            {
                //这部分代码是脱锁后导弹不继续引导了
                //battlefield->missile_list[mindex].lead_state = 0;
                //for ( int rcindex = 0; rcindex < planes[i]._raderCaptureCount; ++rcindex )
                //{
                //  if ( planes[i]._raderCapturePlanesID[rcindex] == battlefield->missile_list[mindex].p_target_air->Sim_id )
                //  {
                battlefield->missile_list[mindex].lead_state = 1;
                //      break;
                //  }
                //}
                if (planes[i]._equipmentType == EquipmentType_E::FRIGATE1)
                    battlefield->missile_list[mindex].Run_SeaSkimming(dt);
                else
                    battlefield->missile_list[mindex].Run(dt);
                planes[i]._missileState[planesMissileIndex]._longitude = battlefield->missile_list[mindex].coordinate_longitude;
                planes[i]._missileState[planesMissileIndex]._latitude = battlefield->missile_list[mindex].coordinate_latitude;
                planes[i]._missileState[planesMissileIndex]._altitude = battlefield->missile_list[mindex].coordinate_altitude;
            }

            planes[i]._missileState[planesMissileIndex]._missile_live = battlefield->missile_list[mindex].missile_live;
            planes[i]._missileState[planesMissileIndex]._radar_state = battlefield->missile_list[mindex].self_lead;
            //planes[i]._missileState[planesMissileIndex]._radar_state = battlefield->missile_list[mindex].radar_state;

            if (!planes[i]._missileState[planesMissileIndex]._isHit)
            {
                Aircraft_Object_C* objPtr = getAircraftObjectByID(planes[i]._targetID);
                //判断命中（2026.1.23修改，增加目标判断，防止目标已死亡）
                if (objPtr != nullptr && GetDistance(planes[i]._missileState[planesMissileIndex]._longitude, planes[i]._missileState[planesMissileIndex]._latitude, objPtr->coordinate_longitude, objPtr->coordinate_latitude) < 20000)
                {
                    battlefield->missile_list[mindex].missile_live = -2;
                    battlefield->missile_list[mindex].lead_state = 0;
                    planes[i]._missileState[planesMissileIndex]._isHit = true;
                }
            }

            planesMissileIndex++;

            string color;
            if (battlefield->missile_list[mindex].base_team == 1) {
                color = "Red";
            }
            else if (battlefield->missile_list[mindex].base_team == 2) {
                color = "Blue";
            }

            //发送显示
            tacview_show->OneFrameMissileState(battlefield->aircraft_count + mindex, battlefield->missile_list[mindex].Sim_id, color,
                battlefield->missile_list[mindex].missile_live, battlefield->missile_list[mindex].coordinate_longitude,
                battlefield->missile_list[mindex].coordinate_latitude, battlefield->missile_list[mindex].coordinate_altitude,
                battlefield->missile_list[mindex].coordinate_roll, battlefield->missile_list[mindex].coordinate_pitch,
                battlefield->missile_list[mindex].coordinate_yaw, battlefield->missile_list[mindex].p_target_air->Sim_id);

            //东大团队
            missStepTime += dt;
            if (missStepTime >= 1)
            {
                battlefield->missile_list[mindex].missile_position_last2 = battlefield->missile_list[mindex].missile_position_last1;
                battlefield->missile_list[mindex].missile_position_last1 = battlefield->missile_list[mindex].missile_position_last;
                battlefield->missile_list[mindex].getEnergyCostMultiple();
                missStepTime = 0;
            }
        }

        setPlaneState(i, &(planes[i]));
        tacview_show->state->object[i].locked_target_valid = 0;
        /*tacview_show->state->object[i].radar_mode = battlefield->aircraft_list[i].radar_state > 0 ? 1 : 0;
        tacview_show->state->object[i].radar_valid  = battlefield->aircraft_list[i].radar_state > 0 ? 1 : 0;*/
    }

    tacview_show->state->time = battlefield->time;
    if (acmiFile != nullptr && acmiFile->isOpen())
        acmiFile->Step(*(tacview_show->state));

#if SEA_AIR_ENABLE_TACVIEW_SERVER
    tacview_show->SendOneFrame(battlefield->time);
#endif

    return true;
}

//雷达独立单步解算
void RadarStep(int speed)
{
    return;
    double _battleTime = battlefield->time;
    for (int j = 0; j < speed; j++)
    {
        if (acmiFile != nullptr && acmiFile->isOpen())
            acmiFile->addFrame(_battleTime);

        for (int i = 0; i < planeCount; i++)
        {
            if (planeStates[i]._isAlive == 1 && planeStates[i]._radarMode == RadarMode_E::RM_Normal && planeStates[i]._radarState == RadarState_E::Scan)
            {
                _battleTime += 0.002;
                tacview_show->state->time = _battleTime;

                planeStates[i]._radar_azimuth = battlefield->aircraft_list[i]._radar_azimuth = getRadarUpdateRadarInfo(&(planeStates[i]));
                tacview_show->OneFrameRadarState(
                    i,
                    planeStates[i]._planeID,
                    planeStates[i]._isAlive,
                    planeStates[i]._radarState,
                    -1,
                    battlefield->aircraft_list[i]._radar_azimuth,
                    battlefield->aircraft_list[i]._radar_elevation,
                    planeStates[i]._radar_range,
                    planeStates[i]._radar_hBeamWidth,
                    planeStates[i]._radar_vBeamWidth,
                    planeStates[i]._radar_all_hBeamWidth,
                    planeStates[i]._radar_all_vBeamWidth,
                    planeStates[i]._radar_all_range,
                    planeStates[i]._radar_all_azimuth,
                    planeStates[i]._radar_all_elevation
                );

                execRadarScan(i);
                execRadarScan_1(i);
            }

            if (planeStates[i]._isAlive == 1 && planeStates[i]._radarMode == RadarMode_E::HGSynergy && planeStates[i]._radarState == RadarState_E::Scan)
            {
                setRadarHGSynergyMode1(planeStates + i);
            }
        }
        if (acmiFile != nullptr && acmiFile->isOpen())
            acmiFile->StepRadar(*(tacview_show->state));
#if SEA_AIR_ENABLE_TACVIEW_SERVER
        tacview_show->SendRadarFrame(_battleTime);
#endif
    }
}

//杀死指定飞机
void killPlane(int planeID)
{
    Aircraft_Object_C* plane = getAircraftObjectByID(planeID);
    if (plane == nullptr)
        return;

    plane->base_live = 0;
}

void StartJoystick()
{
    JoystickInit(&joyinfoex);
}

int OutputJoystick(PlaneState_S* planes)
{
    return Joystick_OutPut(&joyinfoex, planes, 0.01);
}

void SendShowDist(ShowDist* showDist)
{
#if SEA_AIR_ENABLE_WGUA
    SetData((char*)showDist, sizeof(ShowDist));
#endif
}

void SetRadarMode(PlaneState_S* selfPlane, RadarMode_E radarMode, RadarState_E radarState)
{
    selfPlane->_isRadarModeChange = true;
    //SHOW_RADAR_ALL_HBEAMWIDTH = true;
    //SHOW_RADAR_ALL_VBEAMWIDTH = true;
    //SHOW_RADAR_ALL_RANGE    = true;
    SHOW_TACVIEW_RADAR = true;

    setRadarNormalMode(selfPlane);
    switch (radarMode)
    {
    case RadarMode_E::HGSynergy:
        setRadarHGSynergyMode(selfPlane);
        break;
    case RadarMode_E::HGSingle:
        setRadarHGSingleMode(selfPlane);
        break;
    case RadarMode_E::ESMSynergy:
        setRadarESMSynergyMode(selfPlane);
        break;
    }
}

void setRadarNormalMode(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || !selfPlane->_isAlive)
        return;

    if (selfPlane->_radarMode != RadarMode_E::RM_Normal)
    {
        for (int i = 0; i < selfPlane->_friendCount; i++)
        {
            PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[i], selfPlane);
            if (friendPlane == nullptr || !friendPlane->_isAlive)
                continue;
            friendPlane->_HGSynergy._isOpen = false;
            friendPlane->_ESMSynergy._isOpen = false;
            friendPlane->_ESMSynergy._isUsable = false;
            friendPlane->_radarMode = RadarMode_E::RM_Normal;
            friendPlane->_radarState = RadarState_E::Scan;

            Unit_Object_Limit* limit = UNIT_OBJECT_LIMITS[friendPlane->_limitIndex];
            friendPlane->_radar_range = limit->radar_range;
            friendPlane->_radar_view_range = limit->radar_view_range;
            friendPlane->_radar_hBeamWidth = limit->radar_hBeamWidth;
            friendPlane->_radar_vBeamWidth = fabs(limit->radar_vBeamWidth_upper) + fabs(limit->radar_vBeamWidth_below);
            friendPlane->_radar_vBeamWidth_upper = limit->radar_vBeamWidth_upper;
            friendPlane->_radar_vBeamWidth_below = limit->radar_vBeamWidth_below;
            friendPlane->_radar_all_hBeamWidth = limit->radar_all_hbeamwidth;
            friendPlane->_radar_all_vBeamWidth = limit->radar_all_vbeamwidth;
            friendPlane->_radar_all_range = limit->radar_view_range;

            friendPlane->_radar_azimuth = 0;
            friendPlane->_radar_elevation = 0;
            friendPlane->_radar_all_azimuth = 0;
            friendPlane->_radar_all_elevation = 0;

            friendPlane->_raderCaptureCount = 0;
            memset(friendPlane->_raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));
            getPlaneStateByID(friendPlane->_planeID)->_raderCaptureCount = 0;
            memset(getPlaneStateByID(friendPlane->_planeID)->_raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));
        }
    }

    selfPlane->_HGSynergy._isOpen = false;
    selfPlane->_radarMode = RadarMode_E::RM_Normal;
    selfPlane->_radarState = RadarState_E::Scan;
    Unit_Object_Limit* limit = UNIT_OBJECT_LIMITS[selfPlane->_limitIndex];
    selfPlane->_radar_range = limit->radar_range;
    selfPlane->_radar_view_range = limit->radar_view_range;
    selfPlane->_radar_hBeamWidth = limit->radar_hBeamWidth;
    selfPlane->_radar_vBeamWidth = fabs(limit->radar_vBeamWidth_upper) + fabs(limit->radar_vBeamWidth_below);
    selfPlane->_radar_vBeamWidth_upper = limit->radar_vBeamWidth_upper;
    selfPlane->_radar_vBeamWidth_below = limit->radar_vBeamWidth_below;
    selfPlane->_radar_all_hBeamWidth = limit->radar_all_hbeamwidth;
    selfPlane->_radar_all_vBeamWidth = limit->radar_all_vbeamwidth;
    selfPlane->_radar_all_range = limit->radar_view_range;

    selfPlane->_radar_azimuth = 0;
    selfPlane->_radar_elevation = 0;
    selfPlane->_radar_all_azimuth = 0;
    selfPlane->_radar_all_elevation = 0;

    selfPlane->_raderCaptureCount = 0;
    memset(selfPlane->_raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));
    getPlaneStateByID(selfPlane->_planeID)->_raderCaptureCount = 0;
    memset(getPlaneStateByID(selfPlane->_planeID)->_raderCapturePlanesID, 0, Max_Plane_Count * sizeof(int));
}

void setRadarHGSynergyMode(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || selfPlane->_isAlive != 1 || selfPlane->_friendCount != 1)
        return;

    PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[0], selfPlane);
    if (friendPlane == nullptr || friendPlane->_isAlive != 1)
        return;

    double newLength = selfPlane->_radar_range * 1.5;
    std::pair<double, double> a_rp, a_cp, a_lp;
    std::pair<double, double> b_rp, b_cp, b_lp;

    a_rp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw + selfPlane->_radar_hBeamWidth / 2, newLength);
    a_cp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw, newLength);
    a_lp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw - selfPlane->_radar_hBeamWidth / 2, newLength);

    b_rp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw + friendPlane->_radar_hBeamWidth / 2, newLength);
    b_cp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw, newLength);
    b_lp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw - friendPlane->_radar_hBeamWidth / 2, newLength);

    std::pair<double, double> p, p1, p2, p3;
    double self_x = 0, friend_x = 0, cz = 0, selfzj = 0, friendzj = 0;
    Eigen::Vector2d a, b, c, d, p2p;

    //int quadrant = GetQuadrant(selfPlane->_yaw);
    a = Eigen::Vector2d(selfPlane->_longitude, selfPlane->_latitude);
    c = Eigen::Vector2d(friendPlane->_longitude, friendPlane->_latitude);

    if (CrossProduct(selfPlane->_longitude, selfPlane->_altitude, friendPlane->_longitude, friendPlane->_altitude) > 0)
    {
        self_x = GetDistance(selfPlane->_longitude, selfPlane->_latitude, b_lp.first, b_lp.second);
        friend_x = GetDistance(friendPlane->_longitude, friendPlane->_latitude, a_rp.first, a_rp.second);
        //if ( quadrant == 1 || quadrant == 2 )
        b = Eigen::Vector2d(a_lp.first, a_lp.second);
        d = Eigen::Vector2d(b_rp.first, b_rp.second);

        if (!intersection(a, b, c, d))
        {
            //else if ( quadrant == 3 || quadrant == 4 )
            b = Eigen::Vector2d(a_rp.first, a_rp.second);
            d = Eigen::Vector2d(b_lp.first, b_lp.second);
            if (!intersection(a, b, c, d))
            {
                return;
            }
        }
    }
    else
    {
        self_x = GetDistance(selfPlane->_longitude, selfPlane->_latitude, b_rp.first, b_rp.second);
        friend_x = GetDistance(friendPlane->_longitude, friendPlane->_latitude, a_lp.first, a_lp.second);
        //if ( quadrant == 1 || quadrant == 2 )
        b = Eigen::Vector2d(a_rp.first, a_rp.second);
        d = Eigen::Vector2d(b_lp.first, b_lp.second);

        if (!intersection(a, b, c, d))
        {
            //else if ( quadrant == 3 || quadrant == 4 )
            b = Eigen::Vector2d(a_lp.first, a_lp.second);
            d = Eigen::Vector2d(b_rp.first, b_rp.second);
            if (!intersection(a, b, c, d))
            {
                return;
            }
        }
    }

    if (self_x > friend_x)
    {
        p.first = selfPlane->_longitude;
        p.second = selfPlane->_latitude;
        p1.first = friendPlane->_longitude;
        p1.second = friendPlane->_latitude;
        p2.first = b_cp.first;
        p2.second = b_cp.second;

        p3 = PerpendicularFoot(p, p1, p2);
        cz = GetDistance(p3.first, p3.second, friendPlane->_longitude, friendPlane->_latitude);
    }
    else
    {
        p.first = friendPlane->_longitude;
        p.second = friendPlane->_latitude;
        p1.first = selfPlane->_longitude;
        p1.second = selfPlane->_latitude;
        p2.first = a_cp.first;
        p2.second = a_cp.second;

        p3 = PerpendicularFoot(p, p1, p2);
        cz = GetDistance(p3.first, p3.second, selfPlane->_longitude, selfPlane->_latitude);
    }

    if (fabs(self_x - friend_x) < 1000)
        selfzj = friendzj = 0;
    else if (self_x > friend_x)
        selfzj = cz;
    else if (friend_x > self_x)
        friendzj = cz;

    p2p = GetIntersection2(a, b, c, d);

    std::pair<double, double> p4 = PerpendicularFoot(Vector2dToPair(b), Vector2dToPair(p2p), Vector2dToPair(d));

    double p2pSelfLen = GetDistance(p2p.x(), p2p.y(), b.x(), b.y());
    double p2pFriendLen = GetDistance(p2p.x(), p2p.y(), d.x(), d.y());

    selfPlane->_HGSynergy._isOpen = true;
    selfPlane->_HGSynergy._overlapLength = p2pSelfLen >= p2pFriendLen ? p2pSelfLen : p2pFriendLen;
    selfPlane->_HGSynergy._overlapHAngle = angle(atan(GetDistance(b.x(), b.y(), p4.first, p4.second) / GetDistance(p2p.x(), p2p.y(), b.x(), b.y())));
    selfPlane->_HGSynergy._overlapVAngle = selfPlane->_radar_vBeamWidth;
    selfPlane->_HGSynergy._originX = p2p.x();
    selfPlane->_HGSynergy._originY = p2p.y();
    selfPlane->_HGSynergy._originZ = selfPlane->_altitude > friendPlane->_altitude ? friendPlane->_altitude : selfPlane->_altitude;

    friendPlane->_HGSynergy._isOpen = true;
    friendPlane->_HGSynergy._overlapLength = selfPlane->_HGSynergy._overlapLength;
    friendPlane->_HGSynergy._overlapHAngle = selfPlane->_HGSynergy._overlapHAngle;
    friendPlane->_HGSynergy._overlapVAngle = selfPlane->_radar_vBeamWidth;
    friendPlane->_HGSynergy._originX = p2p.x();
    friendPlane->_HGSynergy._originY = p2p.y();
    friendPlane->_HGSynergy._originZ = selfPlane->_HGSynergy._originZ;

    selfPlane->_HGSynergy._radarLength = newLength + selfzj;
    selfPlane->_radarMode = RadarMode_E::HGSynergy;
    selfPlane->_radarState = RadarState_E::Scan;
    selfPlane->_radar_range = selfPlane->_HGSynergy._radarLength;
    selfPlane->_radar_view_range = selfPlane->_HGSynergy._radarLength;
    selfPlane->_radar_azimuth = 0;
    selfPlane->_radar_elevation = 0;

    friendPlane->_HGSynergy._radarLength = newLength + friendzj;
    friendPlane->_radarMode = RadarMode_E::HGSynergy;
    friendPlane->_radarState = RadarState_E::Scan;
    friendPlane->_radar_range = friendPlane->_HGSynergy._radarLength;
    friendPlane->_radar_view_range = friendPlane->_HGSynergy._radarLength;
    friendPlane->_radar_azimuth = 0;
    friendPlane->_radar_elevation = 0;
}

void setRadarHGSynergyMode1(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || selfPlane->_isAlive != 1 || selfPlane->_friendCount != 1)
        return;

    PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[0], selfPlane);
    if (friendPlane == nullptr || friendPlane->_isAlive != 1)
        return;

    double newLength = selfPlane->_radar_range * 1.5;
    std::pair<double, double> a_rp, a_cp, a_lp;
    std::pair<double, double> b_rp, b_cp, b_lp;

    a_rp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw + selfPlane->_radar_hBeamWidth / 2, newLength);
    a_cp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw, newLength);
    a_lp = LongLatOffset(selfPlane->_longitude, selfPlane->_latitude, selfPlane->_yaw - selfPlane->_radar_hBeamWidth / 2, newLength);

    b_rp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw + friendPlane->_radar_hBeamWidth / 2, newLength);
    b_cp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw, newLength);
    b_lp = LongLatOffset(friendPlane->_longitude, friendPlane->_latitude, friendPlane->_yaw - friendPlane->_radar_hBeamWidth / 2, newLength);

    std::pair<double, double> p, p1, p2, p3;
    double self_x = 0, friend_x = 0, cz = 0, selfzj = 0, friendzj = 0;
    Eigen::Vector2d a, b, c, d, p2p;

    //int quadrant = GetQuadrant(selfPlane->_yaw);
    a = Eigen::Vector2d(selfPlane->_longitude, selfPlane->_latitude);
    c = Eigen::Vector2d(friendPlane->_longitude, friendPlane->_latitude);

    if (CrossProduct(selfPlane->_longitude, selfPlane->_altitude, friendPlane->_longitude, friendPlane->_altitude) > 0)
    {
        self_x = GetDistance(selfPlane->_longitude, selfPlane->_latitude, b_lp.first, b_lp.second);
        friend_x = GetDistance(friendPlane->_longitude, friendPlane->_latitude, a_rp.first, a_rp.second);
        //if ( quadrant == 1 || quadrant == 2 )
        b = Eigen::Vector2d(a_lp.first, a_lp.second);
        d = Eigen::Vector2d(b_rp.first, b_rp.second);

        if (!intersection(a, b, c, d))
        {
            //else if ( quadrant == 3 || quadrant == 4 )
            b = Eigen::Vector2d(a_rp.first, a_rp.second);
            d = Eigen::Vector2d(b_lp.first, b_lp.second);
            if (!intersection(a, b, c, d))
            {
                return;
            }
        }
    }
    else
    {
        self_x = GetDistance(selfPlane->_longitude, selfPlane->_latitude, b_rp.first, b_rp.second);
        friend_x = GetDistance(friendPlane->_longitude, friendPlane->_latitude, a_lp.first, a_lp.second);
        //if ( quadrant == 1 || quadrant == 2 )
        b = Eigen::Vector2d(a_rp.first, a_rp.second);
        d = Eigen::Vector2d(b_lp.first, b_lp.second);

        if (!intersection(a, b, c, d))
        {
            //else if ( quadrant == 3 || quadrant == 4 )
            b = Eigen::Vector2d(a_lp.first, a_lp.second);
            d = Eigen::Vector2d(b_rp.first, b_rp.second);
            if (!intersection(a, b, c, d))
            {
                return;
            }
        }
    }

    if (self_x > friend_x)
    {
        p.first = selfPlane->_longitude;
        p.second = selfPlane->_latitude;
        p1.first = friendPlane->_longitude;
        p1.second = friendPlane->_latitude;
        p2.first = b_cp.first;
        p2.second = b_cp.second;

        p3 = PerpendicularFoot(p, p1, p2);
        cz = GetDistance(p3.first, p3.second, friendPlane->_longitude, friendPlane->_latitude);
    }
    else
    {
        p.first = friendPlane->_longitude;
        p.second = friendPlane->_latitude;
        p1.first = selfPlane->_longitude;
        p1.second = selfPlane->_latitude;
        p2.first = a_cp.first;
        p2.second = a_cp.second;

        p3 = PerpendicularFoot(p, p1, p2);
        cz = GetDistance(p3.first, p3.second, selfPlane->_longitude, selfPlane->_latitude);
    }

    if (fabs(self_x - friend_x) < 1000)
        selfzj = friendzj = 0;
    else if (self_x > friend_x)
        selfzj = cz;
    else if (friend_x > self_x)
        friendzj = cz;

    p2p = GetIntersection2(a, b, c, d);

    std::pair<double, double> p4 = PerpendicularFoot(Vector2dToPair(b), Vector2dToPair(p2p), Vector2dToPair(d));

    double p2pSelfLen = GetDistance(p2p.x(), p2p.y(), b.x(), b.y());
    double p2pFriendLen = GetDistance(p2p.x(), p2p.y(), d.x(), d.y());

    selfPlane->_HGSynergy._isOpen = true;
    selfPlane->_HGSynergy._overlapLength = p2pSelfLen >= p2pFriendLen ? p2pSelfLen : p2pFriendLen;
    selfPlane->_HGSynergy._overlapHAngle = angle(atan(GetDistance(b.x(), b.y(), p4.first, p4.second) / GetDistance(p2p.x(), p2p.y(), b.x(), b.y())));
    selfPlane->_HGSynergy._overlapVAngle = selfPlane->_radar_vBeamWidth;
    selfPlane->_HGSynergy._originX = p2p.x();
    selfPlane->_HGSynergy._originY = p2p.y();
    selfPlane->_HGSynergy._originZ = selfPlane->_altitude > friendPlane->_altitude ? friendPlane->_altitude : selfPlane->_altitude;

    friendPlane->_HGSynergy._isOpen = true;
    friendPlane->_HGSynergy._overlapLength = selfPlane->_HGSynergy._overlapLength;
    friendPlane->_HGSynergy._overlapHAngle = selfPlane->_HGSynergy._overlapHAngle;
    friendPlane->_HGSynergy._overlapVAngle = selfPlane->_radar_vBeamWidth;
    friendPlane->_HGSynergy._originX = p2p.x();
    friendPlane->_HGSynergy._originY = p2p.y();
    friendPlane->_HGSynergy._originZ = selfPlane->_HGSynergy._originZ;

    selfPlane->_HGSynergy._radarLength = newLength + selfzj;
    selfPlane->_radarMode = RadarMode_E::HGSynergy;
    selfPlane->_radarState = RadarState_E::Scan;
    selfPlane->_radar_range = selfPlane->_HGSynergy._radarLength;
    selfPlane->_radar_view_range = selfPlane->_HGSynergy._radarLength;
    selfPlane->_radar_azimuth = 0;
    selfPlane->_radar_elevation = 0;

    friendPlane->_HGSynergy._radarLength = newLength + friendzj;
    friendPlane->_radarMode = RadarMode_E::HGSynergy;
    friendPlane->_radarState = RadarState_E::Scan;
    friendPlane->_radar_range = friendPlane->_HGSynergy._radarLength;
    friendPlane->_radar_view_range = friendPlane->_HGSynergy._radarLength;
    friendPlane->_radar_azimuth = 0;
    friendPlane->_radar_elevation = 0;
}

void setRadarHGSingleMode(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || !selfPlane->_isAlive)
        return;

    if (selfPlane->_radarMode != RadarMode_E::HGSingle)
    {
        for (int i = 0; i < selfPlane->_friendCount; i++)
        {
            PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[i], selfPlane);
            if (friendPlane == nullptr || !friendPlane->_isAlive)
                continue;
            if (friendPlane->_radarMode != RadarMode_E::HGSingle)
            {
                friendPlane->_radarMode = RadarMode_E::RM_Normal;
                friendPlane->_radarState = RadarState_E::Scan;
                friendPlane->_HGSynergy._isOpen = false;
            }
        }
    }

    selfPlane->_HGSynergy._isOpen = false;
    selfPlane->_radarMode = RadarMode_E::HGSingle;
    selfPlane->_radarState = RadarState_E::Scan;
    selfPlane->_radar_range = 200000;
    selfPlane->_radar_view_range = 200000;
    selfPlane->_radar_hBeamWidth = 60;
    selfPlane->_radar_vBeamWidth = 60;
    selfPlane->_radar_azimuth = 0;
    selfPlane->_radar_elevation = 0;
}

void setRadarESMSynergyMode(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || !selfPlane->_isAlive || !selfPlane->_ESMSynergy._isUsable)
        return;

    PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_ESMSynergy._friendId, selfPlane);
    if (friendPlane == nullptr || !friendPlane->_isAlive)
        return;

    selfPlane->_ESMSynergy._isOpen = true;
    selfPlane->_radarMode = RadarMode_E::RM_Normal;
    selfPlane->_radarState = RadarState_E::Scan;

    friendPlane->_radarMode = RadarMode_E::ESMSynergy;
    friendPlane->_radarState = RadarState_E::Track;
    friendPlane->_ESMSynergy._isOpen = true;
    friendPlane->_ESMSynergy._isUsable = true;
    friendPlane->_ESMSynergy._isMaster = false;
    friendPlane->_ESMSynergy._enemyId = selfPlane->_ESMSynergy._enemyId;
    friendPlane->_ESMSynergy._friendId = selfPlane->_planeID;
}

bool IsHGSynergy(PlaneState_S* selfPlane)
{
    if (selfPlane == nullptr || !selfPlane->_isAlive)
        return false;

    for (int i = 0; i < selfPlane->_friendCount; i++)
    {
        PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[i], selfPlane);
        if (friendPlane == nullptr || !friendPlane->_isAlive)
            continue;

        double yaw360_1 = Covert180To360(selfPlane->_yaw);
        double yaw360_2 = Covert180To360(friendPlane->_yaw);

        if (friendPlane->_altitude < 7000 || selfPlane->_altitude < 7000)
            continue;

        if (fabs(yaw360_1 - yaw360_2) > 10)
            continue;

        if (fabs(selfPlane->_pitch) > 30 || fabs(friendPlane->_pitch) > 30)
            continue;

        if (selfPlane->_pitch != friendPlane->_pitch)
        {
            //if ( selfPlane->_pitch < 0 && friendPlane->_pitch < 0 )
            //{
            //  if ( fabs(fabs(selfPlane->_pitch) - fabs(friendPlane->_pitch)) > 10 )
            //      continue;
            //}
            //else
            //{
            if (fabs(selfPlane->_pitch - friendPlane->_pitch) > 10)
                continue;
            //}
        }

        if (GetDistance(selfPlane->_longitude, selfPlane->_latitude,
            friendPlane->_longitude, friendPlane->_latitude) > 50000)
            continue;

        if (isRaderCapture(selfPlane->_planeID, friendPlane->_planeID))
            continue;

        if (isRaderCapture(friendPlane->_planeID, selfPlane->_planeID))
            continue;

        double radarHeight11 = selfPlane->_radar_range * tan(rad(selfPlane->_radar_vBeamWidth / 2));
        double radarHeight12 = sin(rad((selfPlane->_radar_vBeamWidth / 2) + fabs(selfPlane->_pitch))) * (sqrt(pow(selfPlane->_radar_range, 2) + pow(radarHeight11, 2)));
        radarHeight12 = selfPlane->_pitch < 0 ? -radarHeight12 : radarHeight12;
        double radarHeight13 = selfPlane->_altitude + radarHeight12;

        double radarHeight21 = friendPlane->_radar_range * tan(rad(friendPlane->_radar_vBeamWidth / 2));
        double radarHeight22 = sin(rad((friendPlane->_radar_vBeamWidth / 2) + fabs(friendPlane->_pitch))) * (sqrt(pow(friendPlane->_radar_range, 2) + pow(radarHeight21, 2)));
        radarHeight22 = friendPlane->_pitch < 0 ? -radarHeight22 : radarHeight22;
        double radarHeight23 = friendPlane->_altitude + radarHeight22;

        if ((radarHeight11 * 2 * 0.3) < fabs(radarHeight13 - radarHeight23) &&
            (radarHeight21 * 2 * 0.3) < fabs(radarHeight13 - radarHeight23))
            continue;

        return true;
    }
    return false;
}

bool IsESMSynergy(PlaneState_S* selfPlane, int enemyPlaneID)
{
    if (selfPlane == nullptr || !selfPlane->_isAlive)
        return false;

    PlaneState_S* enemyPlane = getExternalPlaneStateByID(enemyPlaneID, selfPlane);
    if (enemyPlane == nullptr || !enemyPlane->_isAlive)
        return false;

    if (selfPlane->_ESMSynergy._friendId != 0)
    {
        PlaneState_S* friendPlane = getExternalPlaneStateByID(enemyPlaneID, selfPlane);
        if (friendPlane != nullptr)
            memset(&(friendPlane->_ESMSynergy), 0, sizeof(ESMSynergy_S));
    }
    memset(&(selfPlane->_ESMSynergy), 0, sizeof(ESMSynergy_S));

    double self2Enemy = GetDistance(selfPlane->_longitude, selfPlane->_latitude, enemyPlane->_longitude, enemyPlane->_latitude);
    double rSquare = pow(selfPlane->_radar_range, 2);

    for (int i = 0; i < selfPlane->_friendCount; i++)
    {
        PlaneState_S* friendPlane = getPlaneStateByID(selfPlane->_friendList[i]);
        if (friendPlane == nullptr || !friendPlane->_isAlive)
            continue;

        double friend2Enemy = GetDistance(friendPlane->_longitude, friendPlane->_latitude, enemyPlane->_longitude, enemyPlane->_latitude);
        /*if ( self2Enemy > friend2Enemy )
            continue;*/

        double r1xr2 = self2Enemy * friend2Enemy;
        /*if ( r1xr2 > rSquare * 0.8 && r1xr2 < rSquare * 1.2 )
        {*/
        selfPlane->_ESMSynergy._isOpen = false;
        selfPlane->_ESMSynergy._isUsable = true;
        selfPlane->_ESMSynergy._isMaster = true;
        selfPlane->_ESMSynergy._enemyId = enemyPlaneID;
        selfPlane->_ESMSynergy._friendId = selfPlane->_friendList[i];
        return true;
        //}
    }
    return false;
}

vector<TargetInfo> GetTargetInfo(PlaneState_S* selfPlane, RadarMode_E inRadarMode)
{
#define RANDVAL(ENDVAL) (rand() % (ENDVAL * 2 + 1) - ENDVAL)
    vector<TargetInfo> vct(2);
    vct.clear();

    if (RadarMode_E::HGSynergy == inRadarMode || RadarMode_E::ESMSynergy == inRadarMode)
    {
        for (int i = 0; i < selfPlane->_raderCaptureCount; i++)
        {
            TargetInfo outTargetInfo;
            memset(&outTargetInfo, 0, sizeof(TargetInfo));

            PlaneState_S* tarPlane = getPlaneStateByID(selfPlane->_raderCapturePlanesID[i]);
            if (tarPlane == nullptr)
                continue;

            outTargetInfo.tID = tarPlane->_planeID;
            outTargetInfo.infoType = 1;
            outTargetInfo.tVE = tarPlane->_velocity_east + RANDVAL(50);
            outTargetInfo.tVN = tarPlane->_velocity_north + RANDVAL(50);
            outTargetInfo.tVS = tarPlane->_velocity_downward + RANDVAL(50);
            outTargetInfo.tYaw = tarPlane->_yaw + RANDVAL(10);
            outTargetInfo.tPitch = tarPlane->_pitch + RANDVAL(10);
            outTargetInfo.tRoll = tarPlane->_roll + RANDVAL(10);
            outTargetInfo.tLongitude = tarPlane->_longitude + RANDVAL(100) / 1000.0;
            outTargetInfo.tLatitude = tarPlane->_latitude + RANDVAL(100) / 1000.0;
            outTargetInfo.tAltitutde = tarPlane->_altitude + RANDVAL(100) / 1000.0;

            outTargetInfo.tmDict = GetDistance(selfPlane->_longitude, selfPlane->_latitude, tarPlane->_longitude, tarPlane->_latitude) + RANDVAL(1000);
            outTargetInfo.tmAzi = angle(atan((tarPlane->_latitude - selfPlane->_latitude) / (tarPlane->_longitude - selfPlane->_longitude))) + RANDVAL(10);
            outTargetInfo.tmPitch = angle(atan2((tarPlane->_altitude - selfPlane->_altitude), sqrt(pow(fabs(tarPlane->_longitude - selfPlane->_longitude), 2) + pow(fabs(tarPlane->_latitude - selfPlane->_latitude), 2)) * LatitudeToM)) + RANDVAL(10);

            vct.push_back(outTargetInfo);
        }
    }
    else if (RadarMode_E::HGSingle == inRadarMode)
    {
        for (int i = 0; i < selfPlane->_raderCaptureCount; i++)
        {
            TargetInfo outTargetInfo;
            memset(&outTargetInfo, 0, sizeof(TargetInfo));

            PlaneState_S* tarPlane = getPlaneStateByID(selfPlane->_raderCapturePlanesID[i]);
            if (tarPlane == nullptr)
                continue;

            outTargetInfo.tID = tarPlane->_planeID;
            outTargetInfo.infoType = 0;
            outTargetInfo.tVE = tarPlane->_velocity_east;
            outTargetInfo.tVN = tarPlane->_velocity_north;
            outTargetInfo.tVS = tarPlane->_velocity_downward;
            outTargetInfo.tYaw = tarPlane->_yaw;
            outTargetInfo.tPitch = tarPlane->_pitch;
            outTargetInfo.tRoll = tarPlane->_roll;
            outTargetInfo.tLongitude = tarPlane->_longitude;
            outTargetInfo.tLatitude = tarPlane->_latitude;
            outTargetInfo.tAltitutde = tarPlane->_altitude;

            outTargetInfo.tmDict = GetDistance(selfPlane->_longitude, selfPlane->_latitude, tarPlane->_longitude, tarPlane->_latitude);
            outTargetInfo.tmAzi = angle(atan((tarPlane->_latitude - selfPlane->_latitude) / (tarPlane->_longitude - selfPlane->_longitude)));
            outTargetInfo.tmPitch = angle(atan2((tarPlane->_altitude - selfPlane->_altitude), sqrt(pow(fabs(tarPlane->_longitude - selfPlane->_longitude), 2) + pow(fabs(tarPlane->_latitude - selfPlane->_latitude), 2)) * LatitudeToM));

            vct.push_back(outTargetInfo);
        }
    }
    return vct;
}

int GetRelayGuidancePlaneID(PlaneState_S* selfPlane, int missileID)
{
    MissileState_S* msTemp = nullptr;
    for (int i = 0; i < Max_Missile_Count; i++)
    {
        if (selfPlane->_missileState[i]._missileID == missileID)
        {
            if (selfPlane->_missileState[i]._missile_live == 1)
            {
                msTemp = &(selfPlane->_missileState[i]);
                break;
            }
        }
    }

    if (msTemp == nullptr)
        return -1;

    for (int i = 0; i < selfPlane->_friendCount; i++)
    {
        PlaneState_S* friendPlane = getExternalPlaneStateByID(selfPlane->_friendList[i], selfPlane);
        if (friendPlane == nullptr || !friendPlane->_isAlive)
            continue;

        if (friendPlane->_radarMode != RadarMode_E::RM_Normal)
            continue;

        int planeIndex = getPlaneStateIndex(friendPlane->_planeID);
        if (planeIndex == -1)
            continue;

        double tarYaw = getLockTargetYaw(planeIndex, msTemp->_targetID, friendPlane->_radarMode);
        if (tarYaw == LOCKTARGETYAW_NOTFIND)
            continue;
        else
            return friendPlane->_planeID;
    }
    return -1;
}

void SetRelayGuidanceMode(PlaneState_S* selfPlane, int missileID, int friendID)
{
    PlaneState_S* friendPlane = getExternalPlaneStateByID(friendID, selfPlane);
    if (friendPlane == nullptr || !friendPlane->_isAlive)
        return;

    MissileState_S* msTemp = nullptr;
    for (int i = 0; i < selfPlane->_missileCount; i++)
    {
        if (selfPlane->_missileState[i]._missileID == missileID)
        {
            if (selfPlane->_missileState[i]._missile_live == 1)
            {
                msTemp = &(selfPlane->_missileState[i]);
                break;
            }
        }
    }

    if (msTemp == nullptr)
        return;

    msTemp->_ruidanceMode = GuidanceMode_E::Relay;

    setRadarNormalMode(selfPlane);

    friendPlane->_relayGuidance._missileID = missileID;
    friendPlane->_relayGuidance._planeID = selfPlane->_planeID;
    friendPlane->_relayGuidance._targetID = msTemp->_targetID;
    friendPlane->_radarState = RadarState_E::Track;
    friendPlane->_radarMode = RadarMode_E::RelayGuidance;
    friendPlane->_targetID = msTemp->_targetID;
}

void SetEnableHover(PlaneState_S* selfPlane)
{
    if (selfPlane != nullptr && selfPlane->_isAlive == 1)
    {
        //Aircraft_Object_C*p = getAircraftObjectByID(selfPlane->_planeID);
        PlaneState_S* p = getPlaneStateByID(selfPlane->_planeID);
        if (p != nullptr)
            p->_isAlive = selfPlane->_isAlive = -2;
    }

}

void SetDisableHover(PlaneState_S* selfPlane)
{
    if (selfPlane != nullptr)
    {
        //Aircraft_Object_C*p = getAircraftObjectByID(selfPlane->_planeID);
        PlaneState_S* p = getPlaneStateByID(selfPlane->_planeID);
        if (p != nullptr)
            p->_isAlive = selfPlane->_isAlive = 1;
    }
}

static map<int, FriendRaderCapture_S*>  FriendRaderCaptureMap;
FriendRaderCapture_S* GetFriendsRaderCaptureInfo(const PlaneState_S* selfPlane)
{
    FriendRaderCapture_S* frc;
    if (FriendRaderCaptureMap.count(selfPlane->_planeID) == 0)
    {
        frc = new FriendRaderCapture_S;
        FriendRaderCaptureMap[selfPlane->_planeID] = frc;
    }
    else
    {
        frc = FriendRaderCaptureMap[selfPlane->_planeID];
    }

    frc->friendPlanes.clear();
    frc->targetPlanes.clear();

    for (int i = 0; i < selfPlane->_friendCount; i++)
    {
        PlaneState_S* friendPlane = getPlaneStateByID(selfPlane->_friendList[i]);
        if (friendPlane != nullptr && friendPlane->_isAlive == 1)
        {
            frc->friendPlanes.push_back(friendPlane);
            double distance = calculateAircraftDistance(
                selfPlane->_longitude, selfPlane->_latitude, selfPlane->_altitude,
                friendPlane->_longitude, friendPlane->_latitude, friendPlane->_altitude
            );

            //判断距离够就获得雷达捕获目标
            if (distance < 2000000) //200km
            {
                for (int j = 0; j < friendPlane->_raderCaptureCount; j++)
                {
                    PlaneState_S* targetPlane = getPlaneStateByID(friendPlane->_raderCapturePlanesID[j]);
                    if (targetPlane != nullptr)
                    {
                        frc->targetPlanes.push_back(targetPlane);
                    }
                }
            }
        }
    }

    return frc;
}

double PredictTheTimeOfMissileImpact(int airID, int targetID)
{
    Aircraft_Object_C* airObj = getAircraftObjectByID(airID);
    Aircraft_Object_C* targetObj = getAircraftObjectByID(targetID);

    if (airObj == nullptr || targetObj == nullptr)
        return -1.0;

    int res = battlefield->MissileFire_Test(*airObj, *targetObj);

    while (1)
    {
        res = battlefield->missile_list[battlefield->max_object_count - 1].Run_Test();

        if (res == Target_died)
        {
            targetObj->base_live = 1;
            return battlefield->missile_list[battlefield->max_object_count - 1].predictedTime;
        }

        if (res != CS_OK)
            return res;
    }

    return -1.0;
}

// 防御导弹数量，发现高度，发现距离，成功率下限，成功率上限。
static AntiMissileParam_S antiMissileParams[] = {
    {1, 300,  38000,  0.1, 0.15},
    {1, 2000, 130000, 0.15, 0.2},
    {1, 5000, 230000, 0.2, 0.25},
    {1, 8000, 300000, 0.25, 0.3},
    {1, 12000,380000, 0.3,  0.35},
    {1, 15000,450000, 0.35,  0.4 }
};
bool IsHitShip(const MissileState_S& missileObj)
{
    int index = -1;
    for (int i = 0; i < sizeof(antiMissileParams) / sizeof(AntiMissileParam_S); i++)
    {
        if (missileObj._distanceAtLaunch <= antiMissileParams[i]._distanceAtLaunch &&
            missileObj._heightAtLaunch <= antiMissileParams[i]._heightAtLaunch
            )
        {
            index = i;
            break;
        }
    }

    if (index > -1)
    {
        double _Y = 1.0 / antiMissileParams[index]._missileCount;
        double hitRate = pow(antiMissileParams[index]._successRateLowerLimit, _Y) * 100;
        double hitRateUpper = pow(antiMissileParams[index]._successRateUpperLimit, _Y) * 100;
        //cout << hitRateUpper << "|" << hitRate << endl;

        hitRate += (hitRateUpper - hitRate) / 2;
        srand(time(0));
        int randRate = rand() % 100 + 1;
        bool isHit = randRate > round(hitRate);
        //cout << index << "|" << randRate << "|" << hitRate << endl;
        cout << "导弹ID：" << missileObj._missileID << endl;
        cout << "发射时与目标的高度：" << missileObj._heightAtLaunch << endl;
        cout << "发射时与目标的距离：" << missileObj._distanceAtLaunch << endl;
        cout << "目标ID：" << missileObj._targetID << endl;
        cout << "是否能够命中：" << (isHit ? "是" : "否") << endl;
        cout << "==============================" << endl;

        if (isHit)
            return true;
    }

    return false;
}

void OpenCIWS(PlaneState_S* self, double angle)
{
    self->_CIWS._yaw = angle;

    //  tacview_show->OneFrameAddCircle();
    //
    //  //if ( acmiFile->isOpen() )
    //  //acmiFile->AddCircle(*(tacview_show->state));
    //
    //#ifdef TACVIEW_SERVER
    //  tacview_show->SendAddCircleFrame(battlefield->time);
    //#endif


}

void CloseCIWS(int airID)
{

}

// 在 interface.cpp 末尾
// 增加 vn (北向速度) 和 ve (东向速度) 参数
void SetPlanePosition(int planeID, double lon, double lat, double alt, double vn, double ve)
{
    Aircraft_Object_C* plane = getAircraftObjectByID(planeID);
    if (plane == nullptr) return;

    plane->coordinate_longitude = lon;
    plane->coordinate_latitude = lat;
    plane->coordinate_altitude = alt;

    // 【关键修改】不再强制清零，而是设置传入的速度
    plane->velocity_north = vn;
    plane->velocity_east = ve;
    plane->velocity_downward = 0; // 垂直速度可以清零

    // 同步更新显示缓存
    PlaneState_S* pState = getPlaneStateByID(planeID);
    if (pState != nullptr) {
        pState->_longitude = lon;
        pState->_latitude = lat;
        pState->_altitude = alt;
        pState->_velocity_north = vn;
        pState->_velocity_east = ve;
    }
}
