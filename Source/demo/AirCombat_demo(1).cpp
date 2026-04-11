//
//#include "../Tools/interface.h"
//#include "../Tools/WaypointControl.h"
//#include "../Tools/ZHANSHU.h"
//#include <iomanip>
//
////#define _PLANECOUNT       10 //-5
//#define target_xvhao 12
//
//int main()
//{
//    cout << fixed << setprecision(6);
//
//    static PlaneState_S planes[_PLANECOUNT];
//    memset(planes, 0, sizeof(PlaneState_S));
//
//    initialization_all(target_xvhao, planes);
//
//    InitEnv(0, planes, _PLANECOUNT); //战场初始化
//
//    int target_id = planes[target_xvhao]._planeID;
//    //std::vector<int> myPlanesID = { 10014, 10016, 10018,10020 };
//    //std::vector<int> myPlanesID = { 10014, 10016, 10018 };
//    std::vector<int> myPlanesID = { 10014, 10016 };
//    double dt = 0;
//
//    while (EnvStep(0.1, planes)) //战场单步解算
//    {
//        dt += 0.1;
//        RadarStep(6); //雷达独立单步解算（参数越大雷达扫描解算速度越快，此函数是一个可选函数，如果觉得默认雷达扫描解算速度慢可以调用此函数提速。）
//        //更新状态
//        for (int i = 0; i < _PLANECOUNT; i++)
//        {
//            const PlaneState_S* obj = GetPlaneState(10010 + i);
//            memcpy(planes + i, obj, sizeof(PlaneState_S));
//        }
//        if (dt > 5)
//        {
//            //zhanshu_moban(planes, myPlanesID, target_id, 10010, dt);
//
//
//            zhanshu_muban_yule_baowei(planes, myPlanesID, target_id, 10010, dt);
//        }
//
//        //Sleep(5);
//    }
//
//    return 0;
//}