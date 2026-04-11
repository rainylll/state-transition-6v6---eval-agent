#include "WaypointControl.h"

WaypointControl::WaypointControl()
{
    _isWaypointFinish = true;
    _isYawFinish = true;
    _planeState = nullptr;
}

bool WaypointControl::isWaypointFinish()
{
    return _isWaypointFinish && _isYawFinish;
}

/**
*   @brief          设置航路点
*   @param[in]      longitude       经度，单位：米
*   @param[in]      latitude        纬度，单位：米
*   @param[in]      altitude        高度，单位：米
*   @retval         void
*/
void WaypointControl::setWaypoint(PlaneState_S* planeState, double longitude, double latitude, double altitude)
{
    if (planeState == nullptr)
        return;

    _planeState = planeState;
    _planeState->_isWaypointMode = true;

    _tarLongitude = longitude;
    _tarLatitude = latitude;
    _tarAltitude = altitude;
    _isWaypointFinish = false;
    _isYawFinish = false;
    _yaw_state = yaw_adjust_state;

    _yaw_original = _planeState->_yaw;

    if (planeState->_altitude < _tarAltitude)
    {
        if ((_tarAltitude - planeState->_altitude) / planeState->_altitude > 1)
        {
            _yaw_aoa_up_max = 14;
            _pitchBaseStep_up = 0.05;
            _pitchRecoveryValue = 0.6;
        }
        else
        {
            _yaw_aoa_up_max = 10;
            _pitchBaseStep_up = 0.04;
            _pitchRecoveryValue = 0.4;
        }

        _yaw_aoa_down_max = 2;
        _pitchBaseStep_down = -0.02;
    }
    else if (planeState->_altitude > _tarAltitude)
    {
        _yaw_aoa_up_max = 4;
        _yaw_aoa_down_max = 14;

        _pitchBaseStep_up = 0.03;
        _pitchBaseStep_down = -0.04;

        _pitchRecoveryValue = 0.4;
    }
    else
    {
        _yaw_aoa_up_max = 4;
        _yaw_aoa_down_max = 4;

        _pitchBaseStep_up = 0.02;
        _pitchBaseStep_down = -0.04;

        _pitchRecoveryValue = 0.4;
    }
}

void WaypointControl::execWaypoint()
{
    if (_isWaypointFinish)
        return;

    if (_yaw_state == yaw_finish_state)
    {
        correctPitch();
    }
    else if (_yaw_state == yaw_restore_posture)
    {
        restorePosture();
        return;
    }
    else if (sqrt(pow(fabs(_tarLongitude - _planeState->_longitude), 2) + pow(fabs(_tarLatitude - _planeState->_latitude), 2))
        <= sqrt(pow(0.015, 2) + pow(0.015, 2)))
    {
        cout << "到达目的地" << endl;
        /*system("pause"); */
        _planeState->_roll_ctrl = -_roll_step;

        if (_planeState->_pitch > 0 && _planeState->_pitch > _pitchRecoveryValue)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        }
        else if (_planeState->_pitch < 0 && _planeState->_pitch > -_pitchRecoveryValue)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        }

        _yaw_state = yaw_restore_posture;
        _isWaypointFinish = true;  // 确保标记为完成
        return;
    }

    updateLocation();

    double _original360 = _yaw_original;
    double _yaw360 = _planeState->_yaw;
    double _waypointAngle360 = _waypointAngle;

    if (_planeState->_yaw < 0)
        _yaw360 = 360 - fabs(_planeState->_yaw);
    if (_yaw_original < 0)
        _original360 = 360 - fabs(_yaw_original);
    if (_yaw360 == 0 && _yaw_original != 0)
        _yaw360 = 360;

    if (_waypointAngle360 < 0)
        _waypointAngle360 = 360 - fabs(_waypointAngle360);

    double diffYaw = fabs(_waypointAngle360 - _yaw360);

    if (_isYawFinish)
    {
        if (fabs(_waypointAngle360 - _yaw360) > _yawDiff)
        {
            _isYawFinish = false;
            _yaw_state = yaw_adjust_state;
            _yaw_original = _planeState->_yaw;
        }
        else
        {
            _isYawFinish = true;
        }
        return;
    }

    //if (_yaw_state == yaw_adjust_state && diffYaw < _maneuvering_yaw_min)
    //{
    //  _planeState->_yaw = _waypointAngle;
    //  _isYawFinish = true;
    //  _yaw_state = yaw_finish_state;
    //}
    //else
    //{
    normalExecYaw(_original360, _yaw360, _waypointAngle360);
    //}
}

void WaypointControl::normalExecYaw(double _original360, double _yaw360, double _waypointAngle360)
{
    if (_yaw_state == yaw_adjust_state)
    {
        if (_direction == 1)
        {
            _yaw_step = _yawBaseStep;
            _roll_step = _rollBaseStep;
        }
        else if (_direction == -1)
        {
            _yaw_step = -_yawBaseStep;
            _roll_step = -_rollBaseStep;
        }

        if (_tarAltitude > _planeState->_altitude)
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        if (_tarAltitude < _planeState->_altitude)
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        else
            _planeState->_pitch_ctrl = 0;

        _planeState->_roll_ctrl = _roll_step;
        _planeState->_yaw_ctrl = _yaw_step;

        _yaw_state = yaw_correct_state;
    }
    else if (_yaw_state == yaw_correct_state)
    {
        //角度确认
        if (fabs(_waypointAngle360 - _yaw360) < _angleRecoveryValue)
        {
            if (_planeState->_pitch_ctrl < 0 && _planeState->_pitch > 0)
                _planeState->_pitch_ctrl = _pitchBaseStep_down;
            else if (_planeState->_pitch_ctrl > 0 && _planeState->_pitch < 0)
                _planeState->_pitch_ctrl = _pitchBaseStep_up;
            else
                _planeState->_pitch_ctrl = -_planeState->_pitch_ctrl;

            _planeState->_roll_ctrl = -_roll_step;
            _planeState->_yaw_ctrl = -_yaw_step;
            _yaw_state = yaw_last_state;
            return;
        }

        // 横滚实时修正
        if ((_slopeAngle_max - fabs(_planeState->_roll)) <= _angleRecoveryValue && _planeState->_roll_ctrl != 0)
        {
            _planeState->_roll_ctrl = 0;
        }
        else if (_planeState->_roll_ctrl == 0 && fabs(_planeState->_roll) > _slopeAngle_max + _angleRecoveryValue)
        {
            _planeState->_roll_ctrl += -_roll_step;
        }

        correctPitch();
    }
    else if (_yaw_state == yaw_last_state)
    {
        if (_planeState->_roll_ctrl == 0 && _planeState->_pitch_ctrl == 0)
        {
            _planeState->_yaw_ctrl = 0;
            _isYawFinish = true;
            _yaw_state = yaw_finish_state;
            return;
        }

        if (fabs(_planeState->_roll) >= 0 && fabs(_planeState->_roll) < _angleRecoveryValue)
            _planeState->_roll_ctrl = 0;

        if (fabs(_planeState->_pitch) >= 0 && fabs(_planeState->_pitch) < _pitchRecoveryValue)
            _planeState->_pitch_ctrl = 0;
    }
}

void WaypointControl::correctPitch()
{
    // 俯仰实时修正
    if (_tarAltitude - _planeState->_altitude > _altitudeMinDiff)
    {
        if (_planeState->_pitch < 0)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        }
        else if (_planeState->_pitch > _yaw_aoa_up_max)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        }
        else
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        }
    }
    else if (_planeState->_altitude - _tarAltitude > _altitudeMinDiff)
    {
        if (_planeState->_pitch > 0)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        }
        else if (_planeState->_pitch < -_yaw_aoa_down_max)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        }
        else
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        }
    }
    else
    {
        if (_planeState->_pitch_ctrl > 0 && (_yaw_aoa_up_max - fabs(_planeState->_pitch)) <= _pitchRecoveryValue)
        {
            if (_planeState->_pitch < 0)
            {
                _planeState->_pitch_ctrl = _pitchBaseStep_up;
            }
            else if (_planeState->_pitch > 0)
            {
                _planeState->_pitch_ctrl = _pitchBaseStep_down;
            }
            else
            {
                _planeState->_pitch_ctrl = 0;
            }
        }
        else if (_planeState->_pitch_ctrl < 0 && (_yaw_aoa_down_max - fabs(_planeState->_pitch)) <= _pitchRecoveryValue)
        {
            if (_planeState->_pitch < 0)
            {
                _planeState->_pitch_ctrl = _pitchBaseStep_up;
            }
            else if (_planeState->_pitch > 0)
            {
                _planeState->_pitch_ctrl = _pitchBaseStep_down;
            }
            else
            {
                _planeState->_pitch_ctrl = 0;
            }
        }
        else if (_planeState->_pitch > _yaw_aoa_up_max)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_down;
        }
        else if (_planeState->_pitch < -_yaw_aoa_down_max)
        {
            _planeState->_pitch_ctrl = _pitchBaseStep_up;
        }
    }
}

/**
*   @brief          恢复姿态
*/
void WaypointControl::restorePosture()
{
    if (_planeState->_roll_ctrl == 0 && _planeState->_pitch_ctrl == 0)
    {
        _planeState->_yaw_ctrl = 0;
        _isYawFinish = true;
        _isWaypointFinish = true;
        _yaw_state = yaw_finish_state;

        cout << "完成" << endl;
        system("pause");
        return;
    }

    if (fabs(_planeState->_roll) >= 0 && fabs(_planeState->_roll) < _angleRecoveryValue)
        _planeState->_roll_ctrl = 0;

    if (fabs(_planeState->_pitch) >= 0 && fabs(_planeState->_pitch) < _pitchRecoveryValue)
        _planeState->_pitch_ctrl = 0;
}

void WaypointControl::updateLocation()
{
    double abSide = _tarLongitude - _planeState->_longitude;
    double bcSide = _tarLatitude - _planeState->_latitude;

    if (abSide > 0 && bcSide > 0)       //第一象限
    {
        _quadrant = 1;
        _waypointAngle = 90.0 - rad_to_angle(atan(bcSide / abSide));
    }
    else if (abSide < 0 && bcSide > 0)  //第二象限
    {
        _quadrant = 2;
        _waypointAngle = -90.0 - rad_to_angle(atan(bcSide / abSide));
    }
    else if (abSide < 0 && bcSide < 0)  //第三象限
    {
        _quadrant = 3;
        _waypointAngle = -(90 + rad_to_angle(atan(bcSide / abSide)));
    }
    else if (abSide > 0 && bcSide < 0)  //第四象限
    {
        _quadrant = 4;
        _waypointAngle = 90 + fabs(rad_to_angle(atan(bcSide / abSide)));
    }
    else
    {
        _quadrant = 0;
        _waypointAngle = _planeState->_yaw;
    }

    _direction = getYawPoint(_planeState->_yaw, _waypointAngle);
}
void WaypointControl::setSurroundWaypoints(WaypointControl* controls,
    PlaneState_S* planes,
    int firstIndex,
    int lastIndex,
    double centerLon,
    double centerLat,
    double radius,
    double altitude)
{
    if (controls == nullptr || planes == nullptr)
        return;
    if (firstIndex > lastIndex)
        return;

    int count = lastIndex - firstIndex + 1;
    if (count <= 0)
        return;

    for (int k = 0; k < count; ++k)
    {
        int idx = firstIndex + k;
        PlaneState_S* plane = &planes[idx];
        WaypointControl* ctrl = &controls[idx];

        double angleDeg = 90 + 180.0 * (count == 1 ? 0.5 : (double)k / (double)(count - 1));
        double angleRad = angleDeg * M_PI / 180.0;

        // 北(0°): dLat=+radius, dLon=0
        // 东(90°): dLat=0, dLon=+radius
        // 南(180°): dLat=-radius, dLon=0
        // 西(270°): dLat=0, dLon=-radius
        double dLat = radius * sin(angleRad);
        double dLon = radius * cos(angleRad);

        double tarLon = centerLon + dLon;
        double tarLat = centerLat + dLat;
        ctrl->tarLongitude = tarLon;
        ctrl->tarLatitude = tarLat;
        ctrl->tarAltitude = altitude;
        printf("飞机%d包围航路点设置: lon=%f, lat=%f, alt=%f\n", plane->_planeID, tarLon, tarLat, altitude);
    }
}
