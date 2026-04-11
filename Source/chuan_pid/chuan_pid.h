#pragma once
#include <math.h>
#include <stdio.h>
#include <iostream>
#include "../Tools/interface.h"




class PIDController {
public:
    double Kp, Ki, Kd;
    double _prevError;
    double _integral;
    double _outputLimit; // 输出限幅（例如舵角最大值为1.0）
    double current_throttle_cmd = 0.0;
    PIDController(double p, double i, double d, double limit)
        : Kp(p), Ki(i), Kd(d), _prevError(0), _integral(0), _outputLimit(limit) {
    }

    void reset() {
        _prevError = 0;
        _integral = 0;
    }

    void chuan_go_to_target_yaw(PlaneState_S& plane, double desired_yaw, double dt);
    void chuan_go_to_target_speed(PlaneState_S& plane, double desired_speed, double dt);
    double normalizeAngle(double angle);
};

class chuan_contrl {
public:
    PIDController pid_yaw;
    PIDController pid_speed;

    chuan_contrl()
        : pid_yaw(1.5, 0.001, 0.1, 10.0),    // 初始化 yaw (P, I, D, Limit)  //1.5, 0.001, 0.1, 10.0
        pid_speed(2.0, 0.1, 0.5, 100.0)   // 初始化 speed (P, I, D, Limit)
    {
        // 函数体可以为空
    }


    bool chuan_go_position(PlaneState_S& plane, double desired_speed, double target_lon, double target_lat);
};
