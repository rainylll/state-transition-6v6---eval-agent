#include <math.h>
#include "chuan_gen_zhong.h"
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <vector>
#include <queue>
#include <algorithm>
#include "../Tools/interface.h"
#include "chuan_pid.h"


#define M_PI 3.14159265358979323846


double get_distance(double cur_lon, double cur_lat, double target_lon, double target_lat) {
    // 地球半径 (米)，通常取 WGS84 标准的 6378137.0 或平均半径 6371000.0
    const double EARTH_RADIUS = 6378137.0;

    // 将经纬度从角度转换为弧度
    double radLat1 = cur_lat * M_PI / 180.0;
    double radLat2 = target_lat * M_PI / 180.0;
    double radLon1 = cur_lon * M_PI / 180.0;
    double radLon2 = target_lon * M_PI / 180.0;

    // 计算纬度和经度的差值 (弧度)
    double a = radLat1 - radLat2;
    double b = radLon1 - radLon2;

    // Haversine 公式核心部分
    double s = 2 * std::asin(std::sqrt(std::pow(std::sin(a / 2), 2) +
        std::cos(radLat1) * std::cos(radLat2) * std::pow(std::sin(b / 2), 2)));

    // 弧长乘以地球半径得到距离 (米)
    s = s * EARTH_RADIUS;

    return s;


}

double get_bearing(double cur_lon, double cur_lat, double target_lon, double target_lat) {
    double d_lon = target_lon - cur_lon;
    double d_lat = target_lat - cur_lat;

    // 注意：atan2(y, x) 标准数学是逆时针，0度指向X轴(东)。
    // 导航通常是顺时针，0度指向Y轴(北)。
    // 使用 atan2(d_lon, d_lat) 可以直接得到以北为0顺时针旋转的角度(弧度)
    double angle_rad = atan2(d_lon, d_lat);

    return angle_rad * 180.0 / 3.1415926535;
}
//#define PI 3.14159265358979323846
//#define DEG2RAD (PI / 180.0)
//#define RAD2DEG (180.0 / PI)
// ==========================================
// 2. 辅助函数
// ==========================================
// 修改函数声明，增加 current_yaw 参数，给一个默认值防止报错
double GetBearing(double lon1, double lat1, double lon2, double lat2, double current_yaw) {

    //// 1. 定义一个极小的阈值（例如 0.0001度，约等于 10米）
    //const double EPSILON = 1e-2;

    //// 2. 快速检查经纬度差值
    //double d_lon = fabs(lon2 - lon1);
    //double d_lat = fabs(lat2 - lat1);

    //// 3. 如果距离太近，直接返回当前的航向角
    //// 意思就是：离太近了算不准，那就保持现状，别乱转舵
    //if (d_lon < EPSILON && d_lat < EPSILON) {
    //    return current_yaw;
    //}

    // 4. 正常的计算逻辑
    double y = sin((lon2 - lon1) * PI / 180.0) * cos(lat2 * PI / 180.0);
    double x = cos(lat1 * PI / 180.0) * sin(lat2 * PI / 180.0) -
        sin(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) * cos((lon2 - lon1) * PI / 180.0);

    return atan2(y, x) * 180.0 / PI;
}
//double GetBearing(double lon1, double lat1, double lon2, double lat2) {
//    double y = sin((lon2 - lon1) * DEG2RAD) * cos(lat2 * DEG2RAD);
//    double x = cos(lat1 * DEG2RAD) * sin(lat2 * DEG2RAD) -
//        sin(lat1 * DEG2RAD) * cos(lat2 * DEG2RAD) * cos((lon2 - lon1) * DEG2RAD);
//    double bearing = atan2(y, x) * RAD2DEG;
//    return bearing;
//}

void ForceSyncVelocityToYaw(PlaneState_S& craft)
{
    // 1. 获取当前的合速度大小 (Total Speed)
    double speed = sqrt(pow(craft._velocity_north, 2) + pow(craft._velocity_east, 2));

    // 如果速度极小，就不处理，防止除零或抖动
    if (speed < 0.1) return;

    // 2. 获取当前的偏航角 (注意：需要转为弧度)
    // 根据 UnitDefine.h: coordinate_yaw 单位是度，正北为0，顺时针为正
    // 假设：北向是 X轴(cos)，东向是 Y轴(sin) 
    // 数学对应：North = Speed * cos(yaw), East = Speed * sin(yaw)
    double yaw_rad = craft._yaw * M_PI / 180.0;

    // 3. 重新分解速度向量
    // 这样速度的方向就强行变成了和 Yaw 一致
    craft._velocity_north = speed * cos(yaw_rad);
    craft._velocity_east = speed * sin(yaw_rad);

    // 注意：velocity_downward (垂直速度) 不需要改
}

// 计算速度角度 (默认返回弧度)
double calculateVelocityAngle(double vx, double vy) {
    // atan2(y, x) 返回范围是 [-pi, +pi]
    double radians = std::atan2(vy, vx);

    return radians * (180.0 / M_PI);
}

#include <cmath>

/**
 * 判断目标点是否位于当前位置的后方
 * * @param cur_x, cur_y   当前位置
 * @param tar_x, tar_y   目标点位置
 * @param vx, vy         当前速度分量 (或前进方向向量)
 * @param dist_threshold (可选) 距离阈值。
 * - 如果设置 > 0 (例如 200)，则必须同时满足 "在身后" 且 "距离小于该阈值" 才返回 true。
 * - 如果设为 -1 (默认)，则只判断几何位置是否在身后。
 * @return true if target is behind, false otherwise
 */
bool isTargetBehind(double cur_x, double cur_y,
    double tar_x, double tar_y,
    double vx, double vy)
{
    // 1. 计算指向目标的向量
    double vec_to_target_x = tar_x - cur_x;
    double vec_to_target_y = tar_y - cur_y;

    // 2. 计算点积 (Dot Product)
    // 几何意义: A · B = |A||B|cos(theta)
    double dot_product = (vec_to_target_x * vx) + (vec_to_target_y * vy);

    // 3. 判断几何关系
    bool geometry_behind = (dot_product < 0);

    return geometry_behind;

}
