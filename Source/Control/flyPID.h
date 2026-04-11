#pragma once
#include "../Tools/interface.h"
#include <cmath>

#define PI 3.14159265358979323846
#define DEG2RAD (PI / 180.0)
#define RAD2DEG (180.0 / PI)




// -----------------------------------------------------------
// 基础 PID 类 (根据您提供的结构封装)
// -----------------------------------------------------------
class PID {
public:
    double Kp;
    double Ki;
    double Kd;

    double integral;
    double prev_error;

    PID(double p = 0, double i = 0, double d = 0);

    // 计算输出
    double update(double error, double dt);

    // 重置状态
    void reset();
};

// -----------------------------------------------------------
// 飞行控制器类
// -----------------------------------------------------------
class FlightController {
public:
    FlightController();

    // 初始化参数 (写入指定的 PID 参数)
    void init();

    /**
     * @brief 执行控制主函数
     * @param plane         当前飞机状态引用 (会直接修改其 ctrl 变量)
     * @param targetLon     目标经度
     * @param targetLat     目标纬度
     * @param targetHeight  目标高度
     * @param targetSpeed   期望速度
     * @param dt            时间步长
     */
    void run_control(PlaneState_S& plane,
        double targetLon, double targetLat, double targetHeight,
        double targetSpeed,
        double dt);

    double CalculateBearing(double lon1, double lat1, double lon2, double lat2);
    void Loiter(PlaneState_S* plane, double centerLon, double centerLat, double centerAlt, double radius, double speed);    // +1 顺时针, -1 逆时针


private:
    PID pid_speed;
    PID pid_yaw;
    PID pid_pitch;

    // 辅助数学函数
    double normalizeAngle(double angle);

    double CalculateAngleError(double target, double current);
};
