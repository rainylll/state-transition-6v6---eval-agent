#include "chuan_pid.h"
#include "chuan_gen_zhong.h"

double PIDController::normalizeAngle(double angle) {
    while (angle > 180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

void PIDController::chuan_go_to_target_yaw(PlaneState_S& plane, double desired_yaw, double dt) {

    double error = desired_yaw - plane._yaw; // 误差 = 期望速度 - 实际速度TAS_roll_throttle  
    while (error > 180)  error -= 360; //归一化到-180---180
    while (error < -180) error += 360;
    _integral += error * dt;
    double derivative = (error - _prevError) / dt;
    double out = Kp * error + Ki * _integral + Kd * derivative;
    if (out > 0.05) out = 0.05; //限制幅度，每秒0.5度
    if (out < -0.05) out = -0.05;
    /* plane._yaw += out;*/
    // plane._yaw_ctrl = out;
    plane._yaw += out;
    _prevError = error;
}


void PIDController::chuan_go_to_target_speed(PlaneState_S& plane, double desired_speed, double dt) {

    double MAX_THROTTLE = 100; // 你的硬限制0.05
    double MIN_THROTTLE = 0.0;  // 假设不能倒车，如果能倒车设为 -0.05

    double cur_speed = sqrt(pow(plane._velocity_north, 2) + pow(plane._velocity_east, 2));
    double error = desired_speed - cur_speed;
    // 积分
    _integral += error * dt;
    // 积分限幅 (非常重要，防止长时间没达到速度后积分项变得巨大)
    double integral_limit = MAX_THROTTLE * 0.5; // 积分项最多贡献一半油门
    if (_integral > integral_limit / Ki) _integral = integral_limit / Ki;
    if (_integral < -integral_limit / Ki) _integral = -integral_limit / Ki;
    // 微分
    double derivative = (error - _prevError) / dt;
    _prevError = error;
    // 计算目标输出 (Raw Output)
    double out = Kp * error + Ki * _integral + Kd * derivative;
    if (out > MAX_THROTTLE) out = MAX_THROTTLE;
    if (out < MIN_THROTTLE) out = MIN_THROTTLE;

    plane._throttle = out;
}
/*
    plane：船
    desired_speed：期望速度，DEEPSEEK说15m/s是算很大的速度了
    target_lon ：目标点经度
    target_lat ：目标纬度
    调用函数的时候，定义一个chuan_contrl对象在while之前（PID值在默认构造函数里面）

    返回值：  1 --->到达目标位置
              0 --->没到
              2 --->走过了
*/

bool chuan_contrl::chuan_go_position(PlaneState_S& plane, double desired_speed, double target_lon, double target_lat) {

    Waypoint target_pt;
    target_pt.longitude = target_lon;
    target_pt.latitude = target_lat;
    double desired_yaw = GetBearing(plane._longitude, plane._latitude, target_pt.longitude, target_pt.latitude, plane._yaw);
    double dist_deg = get_distance(plane._longitude, plane._latitude, target_pt.longitude, target_pt.latitude);
    int condition2 = isTargetBehind(
        plane._longitude, plane._latitude,
        target_pt.longitude, target_pt.latitude,
        plane._velocity_east, plane._velocity_north);
    bool is_passed = condition2;
    if (is_passed) return 2;
    if (dist_deg < 1000) { // 1000代表距离目标位置的范围，在这个范围内就代表到了
        //std::cout << "到达航点 " << std::endl;
        plane._velocity_north = 0; //这里是想到了目标定停下来，但是好像没用
        plane._velocity_east = 0;
        return true;
    }
    pid_yaw.chuan_go_to_target_yaw(plane, desired_yaw, 0.1);
    pid_speed.chuan_go_to_target_speed(plane, desired_speed, 0.1);
}
