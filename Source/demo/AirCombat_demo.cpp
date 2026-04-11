#include "../Tools/interface.h"
#include "../Tools/WaypointControl.h"
#include "../Tools/RedCommander.h"  
#include "../Tools/BlueCommander.h"
#include <iomanip>

#define _PLANECOUNT     12
#define _MAX_PLANES_BUFFER  100

int main()
{
    cout << fixed << setprecision(6);

    // 1. 分配内存
    PlaneState_S planes[_MAX_PLANES_BUFFER];
    memset(planes, 0, sizeof(planes));

    // 2. 分别初始化
    RedForce::RedCommander::InitRedUnits(planes, _PLANECOUNT);
    BlueForce::BlueCommander::InitBlueUnits(planes, _PLANECOUNT);

    // 3. 仿真环境初始化 (Tacview连接等)
    InitEnv(0, planes, _PLANECOUNT); //战场初始化
    //AddBoundary(1, 116.94136, 24.86444, 7000, 700000, 300000, 20000, Color_E::Violet); //Tacview战场边界效果设置

    // 实例化决策层对象
    RedForce::RedCommander redCmd;
    BlueForce::BlueCommander blueCmd;

    double dt = 0;

    while (EnvStep(0.1, planes)) //战场单步解算
    {
        dt += 0.1;

        // --- 数据同步：从底层引擎获取最新状态 ---
        for (int i = 0; i < _PLANECOUNT; i++)
        {
            int targetID = 0;
            if (i < 6) {
                targetID = 10010 + i;
            }
            else {
                targetID = 20010 + (i - 6);
            }

            const PlaneState_S* obj = GetPlaneState(targetID);
            if (obj) {
                memcpy(planes + i, obj, sizeof(PlaneState_S));
            }
        }


        // === 决策 Update ===
        redCmd.Update(0.1, planes, _PLANECOUNT, dt);
        blueCmd.Update(0.1, planes, _PLANECOUNT, dt);

    }

    return 0;
}
