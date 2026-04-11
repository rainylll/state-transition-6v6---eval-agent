#include "flyPID.h"
#include <iostream>
#include <algorithm> // for std::max, std::min

//==========================================
//PID 实现
//==========================================
PID::PID(double p, double i, double d) : Kp(p), Ki(i), Kd(d), integral(0), prev_error(0) {}

double PID::update(double error, double dt) {
    //if (dt <= 0) return 0;
    integral += error * dt;
    double derivative = (error - prev_error) / dt;
    prev_error = error;
    return Kp * error + Ki * integral + Kd * derivative;
}

void PID::reset() {
    integral = 0;
    prev_error = 0;
}

// ==========================================
// FlightController 实现
// ==========================================
FlightController::FlightController() {
    init();
}

void FlightController::init() {


    // 速度 PID: 0.25, 0.05, 0.4
    pid_speed = PID(0.25, 0.05, 0.4);

    // 偏航 PID: 0.1, 0.5, 0
    pid_yaw = PID(0.1, 0.5, 0);

    // 俯仰 PID: 0.4, 0.25, 0
    pid_pitch = PID(0.05, 0.25, 0);
}

double FlightController::normalizeAngle(double angle) {
    while (angle > 180) angle -= 360;
    while (angle < -180) angle += 360;
    return angle;
}

double FlightController::CalculateAngleError(double target, double current) {
    double error = target - current;
    while (error > 180) error -= 360;
    while (error < -180) error += 360;
    return error;
}

double FlightController::CalculateBearing(double lon1, double lat1, double lon2, double lat2) {
    double y = sin((lon2 - lon1) * DEG2RAD) * cos(lat2 * DEG2RAD);
    double x = cos(lat1 * DEG2RAD) * sin(lat2 * DEG2RAD) -
        sin(lat1 * DEG2RAD) * cos(lat2 * DEG2RAD) * cos((lon2 - lon1) * DEG2RAD);
    double bearing = atan2(y, x) * RAD2DEG;
    return bearing; // atan2 已经处理了象限，结果是 -180~180
}

void Loiter(PlaneState_S& plane,
    double centerLon, double centerLat,
    double radius,
    double speed,
    int direction)     // +1 顺时针, -1 逆时针
{
    // ============================
    // 1. 计算与中心点的相对位置
    // ============================
    //double dN = (plane._latitude - centerLat);   // 北向距离（单位：度，但比值仍可用）
    //double dE = (plane._longitude - centerLon);   // 东向距离
    double lat_rad = centerLat * DEG2RAD;
    double dN = (plane._latitude - centerLat) * DEG2RAD * EARTH_RADIUS;  // 北向距离（单位：度，但比值仍可用）
    double dE = (plane._longitude - centerLon) * DEG2RAD * EARTH_RADIUS * cos(lat_rad);   // 东向距离

    // 转成弧度角（中心->飞机）
    double angle_to_center = atan2(dE, dN);  // [-pi, pi]

    // ============================
    // 2. 圆周切向方向
    // ============================
    // 顺时针：切线方向 = angle_to_center - 90°
    // 逆时针：切线方向 = angle_to_center + 90°
    double desiredYaw = angle_to_center + direction * (-PI / 2.0);

    // ============================
    // 3. 半径误差修正
    // ============================
    double dist = sqrt(dN * dN + dE * dE);   // 当前半径
    double err = dist - radius;          // >0 表示飞太外，<0 飞太内

    // 半径误差 -> 航向角修正 (比例控制)
    double K = 0.002;  // 你可调整
    desiredYaw += -K * err * direction;

    // ============================
    // 4. 角度转为度
    // ============================
    desiredYaw = desiredYaw * 180.0 / PI;

    // 航向角归一化
    while (desiredYaw < 0) desiredYaw += 360;
    while (desiredYaw >= 360) desiredYaw -= 360;

    // ============================
    // 5. 写入状态
    // ============================
    plane._yaw = desiredYaw;
    //return desiredYaw;
}

void FlightController::run_control(PlaneState_S& plane,
    double targetLon, double targetLat, double targetHeight,
    double targetSpeed,
    double dt)
{

    // === 1. 速度控制 (PID 控制油门) ===
    // 误差 = 期望速度 - 实际真空速
    double speed_err = targetSpeed - plane.TAS;
    double throttle_delta = pid_speed.update(speed_err, 0.03);

    plane._throttle += throttle_delta; // 累加控制

    // 油门限幅 (0 ~ 100)
    if (plane._throttle > 100) plane._throttle = 100;
    if (plane._throttle < 0) plane._throttle = 0;

    //cout << targetSpeed << "," << plane.TAS << "," << plane._throttle << endl;
   /*  === 2. 偏航(Yaw) 控制 ===
     计算期望航向*/
    double desired_yaw = CalculateBearing(plane._longitude, plane._latitude, targetLon, targetLat);
    //double desired_yaw = Loiter(plane, targetLon, targetLat, 5000, targetSpeed, 1);
    double yaw_err = CalculateAngleError(desired_yaw, plane._yaw);
    double yaw_cmd = pid_yaw.update(yaw_err, 0.1);
    plane._yaw = yaw_cmd;
    plane._yaw = normalizeAngle(plane._yaw);


    // === 3. 俯仰(Pitch) / 高度控制 ===

    double alt_err = targetHeight - plane._altitude;

    // 将高度误差转换为期望俯仰角 (限制在 +/- 15度)
    double desired_pitch = alt_err * 0.1;
    //double desired_pitch =10;
    if (desired_pitch > 15) desired_pitch = 15;
    if (desired_pitch < -15) desired_pitch = -15;

    double pitch_err = desired_pitch - plane._pitch;
    double pitch_cmd = pid_pitch.update(pitch_err, 0.03);

    plane._pitch = pitch_cmd;
    // }


}

void  FlightController::Loiter(PlaneState_S* plane, double centerLon, double centerLat, double centerAlt, double radius, double speed) {
    // 1. 计算当前角度
    double dy = plane->_latitude - centerLat;
    double dx = plane->_longitude - centerLon;
    double currentAngle = atan2(dy, dx); // 数学角度

    // 计算距离 (米)
    double dist = calculateAircraftDistance(plane->_longitude, plane->_latitude, 0, centerLon, centerLat, 0);

    // 2. 计算修正系数
    // 距离差比率：正数表示太远，负数表示太近
    double errorRatio = (dist - radius) / radius;

    // 限制修正幅度，防止剧烈机动
    if (errorRatio > 0.5) errorRatio = 0.5;
    if (errorRatio < -0.5) errorRatio = -0.5;

    // 3. 计算虚拟目标角度
    // 正常切线是 90度 (PI/2)。
    // 如果太远(error>0)，我们要多转一点指向圆心 -> 角度变大
    // 如果太近(error<0)，我们要少转一点向外飞 -> 角度变小
    // 这里的 1.2 是向心力系数，越大修正越快
    double offsetAngle = (3.14159 / 2.0) + (errorRatio * 1.2);

    // 假设顺时针盘旋：当前角度 - 偏移角
    // (注：根据坐标系不同，可能是 + 或 -，如果发现反向飞需改符号)
    double targetAngle = currentAngle - offsetAngle;

    // 4. 【修复2】拉大诱导距离 (Look Ahead Distance)
    // 之前是 0.05 (5km)，太近了导致转弯过急。
    // 现在改为 0.15 (约15km)，让轨迹更平滑宽大
    double lookAheadDist = 0.15;

    double virtualTargetLon = centerLon + lookAheadDist * cos(targetAngle);
    double virtualTargetLat = centerLat + lookAheadDist * sin(targetAngle);

    // 5. 调用PID控制
    run_control(*plane,
        virtualTargetLon, virtualTargetLat, centerAlt,
        radius,
        0.1);

    // 6. 盘旋时的特殊微调
    // 盘旋不需要全速，省点油，也减小转弯半径压力
    plane->_throttle = 60;
    // 强制改平一点滚转，防止侧滑坠毁
    if (plane->_roll > 25) plane->_roll = 25;
    if (plane->_roll < -25) plane->_roll = -25;
}
