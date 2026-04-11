#pragma once
#include "interface.h"

double fangxiang(double a__latitude, double a_longitude, double b__latitude, double b_longitude);

class WaypointControl
{
public:
    WaypointControl();

    void setWaypoint(PlaneState_S* planeState, double longitude, double latitude, double altitude);
    void execWaypoint();
    bool isWaypointFinish();

    // 2025.11.29新增
    bool getflag = 0;
    double  tarLongitude;
    double  tarLatitude;
    double  tarAltitude;

    inline void updateLocation();

    void restorePosture();
    void normalExecYaw(double _original360, double _yaw360, double _waypointAngle360);

    // 批量分配圆形包围航路点（第一架在正北，其余按顺时针均匀分布）
    // controls: 与飞机索引对应的WaypointControl数组（大小>= lastIndex-firstIndex+1）
    // planes: 飞机状态数组
    // firstIndex/lastIndex: 要分配的飞机在planes数组中的起止索引（含）
    // centerLon/centerLat: 被包围点的经纬度
    // radius: 飞机与中心点的水平距离（同原有代码的offset单位）
    // altitude: 包围时的目标高度
    static void setSurroundWaypoints(WaypointControl* controls,
        PlaneState_S* planes,
        int firstIndex,
        int lastIndex,
        double centerLon,
        double centerLat,
        double radius,
        double altitude);

private:

    void correctPitch();

    typedef enum
    {
        yaw_adjust_state,
        yaw_correct_state,
        yaw_last_state,
        yaw_finish_state,
        yaw_restore_posture,
    } State;

    State   _yaw_state;

    bool    _isYawFinish;
    bool    _isWaypointFinish;
    double  _waypointAngle;                 // 航路点目标角度

    double  _tarLongitude;
    double  _tarLatitude;
    double  _tarAltitude;

    double  _yaw_original;                  // 原始偏航角
    double  _pitch_original;                // 原始俯仰角
    double  _roll_original;                 // 原始横滚角
    double  _altitude_original;             // 原始高度

    double  _slopeAngle_max = 30;           // 最大坡度角
    double  _maneuvering_yaw_min = 10;      // 最小机动偏航角
    double  _yaw_aoa_up_max;                // 调整航向时最大的迎角上限
    double  _yaw_aoa_down_max;              // 调整航向时最大的迎角下限

    double  _yaw_step;
    double  _roll_step;

    const double  _yawDiff = 10;
    const double  _altitudeMinDiff = 200;
    const double  _angleRecoveryValue = 0.6;

    const double  _yawBaseStep = 0.02;
    const double  _rollBaseStep = 0.2;

    const double  _pitchBaseStep = 0;
    double        _pitchBaseStep_up;
    double        _pitchBaseStep_down;
    double        _pitchRecoveryValue;

    int _quadrant;
    int _direction;

    PlaneState_S* _planeState;

    // 多线段绕圆相关
    struct Segment { double lon; double lat; double alt; };
    int    _segmentCount = 0;
    int    _currentSegmentIndex = 0;
    Segment _segments[5];
};

