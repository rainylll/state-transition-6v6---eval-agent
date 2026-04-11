#ifndef SEA_SKIMMING_CALCULATOR_H
#define SEA_SKIMMING_CALCULATOR_H


double getEGM96SeaLevel(double lon, double lat);
void calcSeaSkimmingLLH(double curr_lon, double curr_lat, double curr_h,
    double target_sea_h,  // 目标掠海高度（用户输入）
    double sea_level_h,
    double& new_lon, double& new_lat, double& new_h,
    double& actual_sea_h);

#endif