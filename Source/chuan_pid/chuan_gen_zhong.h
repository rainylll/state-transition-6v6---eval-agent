#pragma once
#define _PLANECOUNT 1
// PI 3.1415926535
#include "../Tools/interface.h"

const double MAP_ORIGIN_LON = 118.29000;
const double MAP_ORIGIN_LAT = 23.53000;
const double GRID_RES = 0.001; // 网格分辨率 (约100米)
const int MAP_WIDTH = 150;     // 150 * 0.001 = 0.15 度
const int MAP_HEIGHT = 150;

struct Waypoint {
    double longitude;
    double latitude;
};

struct Node {
    int x, y;           // 网格坐标
    double g_cost;      // 从起点到当前的代价
    double h_cost;      // 启发式代价(到终点)
    double f_cost;      // 总代价 f = g + h
    Node* parent;       // 父节点，用于回溯路径

    Node(int _x, int _y) : x(_x), y(_y), g_cost(0), h_cost(0), f_cost(0), parent(nullptr) {}

    // 优先队列比较结构 (f_cost 小的优先)
    bool operator>(const Node& other) const {
        return f_cost > other.f_cost;
    }
};
// 定义一个矩形路径
//Waypoint path[] = {
//    {118.29746, 23.53782}, // 起点 (Home)
//    {118.29746, 23.53882}, // 点1：向北移动 ~110m
//    {118.29846, 23.53882}, // 点2：向东移动 ~100m
//    {118.29846, 23.53782}, // 点3：向南移动
//    {118.29746, 23.53782}  // 点4：回到起点
//};


double get_distance(double cur_lon, double cur_lat, double target_lon, double target_lat);
double get_bearing(double cur_lon, double cur_lat, double target_lon, double target_lat);
double GetBearing(double lon1, double lat1, double lon2, double lat2, double current_yaw = 0.0);
void ForceSyncVelocityToYaw(PlaneState_S& craft);
double calculateVelocityAngle(double vx, double vy);
bool isTargetBehind(double cur_x, double cur_y,
    double tar_x, double tar_y,
    double vx, double vy);
