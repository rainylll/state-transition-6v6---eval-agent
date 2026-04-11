#include <iostream>
#include <cmath>
#include <iomanip>
#include <vector>
#include <corecrt_math_defines.h>
#include "sea_skimming.h"

// WGS84椭球参数
const double WGS84_A = 6378137.0;         
const double WGS84_F = 1.0 / 298.257223563; 
const double WGS84_B = WGS84_A * (1 - WGS84_F); 
const double WGS84_E2 = (2 * WGS84_F) - (WGS84_F * WGS84_F); 

// 角度/弧度转换
inline double deg2rad(double deg) { return deg * M_PI / 180.0; }
inline double rad2deg(double rad) { return rad * 180.0 / M_PI; }

// 经纬高转ECEF
void llh2ecef(double lon, double lat, double h, double& x, double& y, double& z) {
    double lon_rad = deg2rad(lon);
    double lat_rad = deg2rad(lat);
    double cos_lat = cos(lat_rad), sin_lat = sin(lat_rad);
    double cos_lon = cos(lon_rad), sin_lon = sin(lon_rad);

    double N = WGS84_A / sqrt(1 - WGS84_E2 * sin_lat * sin_lat);
    x = (N + h) * cos_lat * cos_lon;
    y = (N + h) * cos_lat * sin_lon;
    z = (N * (1 - WGS84_E2) + h) * sin_lat;
}

// ECEF转经纬高
void ecef2llh(double x, double y, double z, double& lon, double& lat, double& h) {
    double p = sqrt(x * x + y * y);
    double theta = atan2(z * WGS84_A, p * WGS84_B);

    double lat_rad = atan2(
        z + (WGS84_E2 * WGS84_B * pow(sin(theta), 3)),
        p - (WGS84_E2 * WGS84_A * pow(cos(theta), 3))
    );
    double cos_lat = cos(lat_rad), sin_lat = sin(lat_rad);
    double N = WGS84_A / sqrt(1 - WGS84_E2 * sin_lat * sin_lat);

    double lon_rad = atan2(y, x);
    h = (p / cos_lat) - N;

    lon = rad2deg(lon_rad);
    lat = rad2deg(lat_rad);
    if (lon < 0) lon += 360.0;
}

// 计算掠海经纬高（新增高度范围约束）
void calcSeaSkimmingLLH(double curr_lon, double curr_lat, double curr_h,
    double target_sea_h,  // 目标掠海高度（用户输入）
    double sea_level_h,
    double& new_lon, double& new_lat, double& new_h,
    double& actual_sea_h) {  // 输出实际使用的掠海高度（约束后）
    // 约束实际掠海高度在 [1, target_sea_h] 范围内
    actual_sea_h = std::max(1.0, std::min(target_sea_h, target_sea_h));

    // 计算目标大地高
    new_h = actual_sea_h + sea_level_h;

    // 坐标转换与调整
    double x, y, z;
    llh2ecef(curr_lon, curr_lat, curr_h, x, y, z);

    double lon_rad = deg2rad(curr_lon), lat_rad = deg2rad(curr_lat);
    double cos_lat = cos(lat_rad), sin_lat = sin(lat_rad);
    double cos_lon = cos(lon_rad), sin_lon = sin(lon_rad);

    double nx = cos_lat * cos_lon, ny = cos_lat * sin_lon, nz = sin_lat;
    double delta_h = new_h - curr_h;

    double x_new = x + delta_h * nx;
    double y_new = y + delta_h * ny;
    double z_new = z + delta_h * nz;

    ecef2llh(x_new, y_new, z_new, new_lon, new_lat, new_h);
}

// EGM96海平面高度查询（保持不变）
struct EGM96Grid {
    double lat;  // 纬度（度）
    double lon;  // 经度（度）
    double und;  // 大地水准面差距（米）
};

const std::vector<EGM96Grid> egm96_grid = {
    {90.0, 0.0, -2.3}, {90.0, 15.0, -2.3}, {90.0, 30.0, -2.2},
    {60.0, 0.0, -1.8}, {60.0, 15.0, -1.7}, {60.0, 30.0, -1.6},
    {30.0, 0.0, -0.5}, {30.0, 15.0, -0.4}, {30.0, 30.0, -0.3},
    {0.0, 0.0, 1.2},   {0.0, 15.0, 1.3},   {0.0, 30.0, 1.4},
    {-30.0, 0.0, 2.1}, {-30.0, 15.0, 2.2}, {-30.0, 30.0, 2.3},
    {-60.0, 0.0, 1.9}, {-60.0, 15.0, 2.0}, {-60.0, 30.0, 2.1},
    {-90.0, 0.0, 1.5}, {-90.0, 15.0, 1.5}, {-90.0, 30.0, 1.5},
    {30.0, 115.0, -2.5}, {30.0, 130.0, -2.8},
    {0.0, 115.0, 2.0}, {0.0, 130.0, 2.2},
    {39.9, 116.4, -2.0}  // 北京附近
};

double getEGM96SeaLevel(double lon, double lat) {
    EGM96Grid near[4];
    int count = 0;

    for (const auto& grid : egm96_grid) {
        double d_lon = fabs(lon - grid.lon);
        double d_lat = fabs(lat - grid.lat);
        d_lon = d_lon > 180.0 ? 360.0 - d_lon : d_lon;
        double dist_sq = d_lon * d_lon + d_lat * d_lat;

        if (count < 4) {
            near[count++] = grid;
        } else {
            int max_idx = 0;
            double max_dist_sq = 0.0;
            for (int i = 0; i < 4; ++i) {
                double dl = fabs(lon - near[i].lon);
                double dlt = fabs(lat - near[i].lat);
                dl = dl > 180.0 ? 360.0 - dl : dl;
                double d_sq = dl * dl + dlt * dlt;
                if (d_sq > max_dist_sq) {
                    max_dist_sq = d_sq;
                    max_idx = i;
                }
            }
            if (dist_sq < max_dist_sq) {
                near[max_idx] = grid;
            }
        }
    }

    if (count == 0) return 0.0;
    if (count == 1) return near[0].und;

    double x0 = near[0].lon, y0 = near[0].lat, z00 = near[0].und;
    double x1 = near[1].lon, y1 = near[1].lat, z10 = near[1].und;
    double x2 = near[2].lon, y2 = near[2].lat, z01 = near[2].und;
    double x3 = near[3].lon, y3 = near[3].lat, z11 = near[3].und;

    double x = (x1 != x0) ? (lon - x0) / (x1 - x0) : 0.5;
    double y = (y2 != y0) ? (lat - y0) / (y2 - y0) : 0.5;
    x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
    y = y < 0.0 ? 0.0 : (y > 1.0 ? 1.0 : y);

    return (1 - x) * (1 - y) * z00 + x * (1 - y) * z10 +
        (1 - x) * y * z01 + x * y * z11;
}
