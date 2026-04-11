#pragma once
#include <vector>
#include <set>
#include <map>
#include "../Tools/interface.h" 
#include "../chuan_pid/chuan_pid.h"
#include "../chuan_pid/chuan_gen_zhong.h"
#include "../Control/flyPID.h"

namespace BlueForce {
    // 任务类型枚举
    enum class UnitTask_E {
        Idle,
        HoverOnCarrier,   // 甲板悬停
        GoToAssemblyArea, // 起飞去集结区
        InAssemblyArea,   // 集结区盘旋
        GoToHoldingArea,  // 前往待战区
        InHoldingArea,    // 待战区盘旋
        Attack,            // 攻击
        ReturnToBase      // 返回基地
    };
    // 待战区信息结构
    struct HoldingArea {
        double lon;
        double lat;
        double alt;
        std::set<int> presentUnitIds; // 存储已抵达此区域的单位ID
    };
    // 单位信息
    struct UnitInfo {
        int index;          // 原始数组下标
        int id;             // ID
        bool isAlive;       // 存活状态
        int missileCount;
        double lon, lat;    // 当前位置

        // === 新增：任务状态 ===
        UnitTask_E task;    // 当前任务
        double targetLon;   // 目标经度
        double targetLat;   // 目标纬度
    };
    // 战术类型（对应 ZHANSHU.cpp 三个函数）
    enum class TacticType_E {
        None,
        YouDao,   // zhanshu_youdao
        BaoWei_low_altitude,    // zhanshu__baowei_low_altitude
        QiShe,      //zhanshu_qishe
        BaoWei_high_altitude //zhanshu_baowei_high_altitude
    };
    // 战术分配结果（MakeTacticalDecisions 输出的“指令”）
    struct ZoneAssignment {
        int zoneId;
        TacticType_E tactic;
        std::vector<int> planeIds; // 由该区调配的飞机ID（用于执行战术）
        int targetId;              // 这个区的目标（敌方ID）
    };
    // 舰队状态
    struct FleetStatus {
        UnitInfo carrier;
        std::vector<UnitInfo> fighters;
        std::vector<UnitInfo> guards;
        std::set<int> detectedEnemies;

        void Reset() {
            fighters.clear();
            guards.clear();
            detectedEnemies.clear();
        }
    };
    class BlueCommander {
    public:
        BlueCommander();

        // 静态场景初始化函数 
        static void InitBlueUnits(PlaneState_S* planes, int count);

        // 核心循环
        void Update(double dt, PlaneState_S* planes, int count, double dtt);

    private:
        // --- 感知 ---
        void GatherFleetInfo(PlaneState_S* planes, int count);

        // --- 决策 ---
        void ManageCarrierLaunchCycle(double dt, PlaneState_S* planes);  // 航母起降调度
        void MakeStrategicDecisions(PlaneState_S* planes, int count);    // 总体战略决策

        // 只计算待战区
        void CalculateHoldingAreas();                                  // 计算待战区

        // 随机战术分配
        void MakeTacticalDecisions(PlaneState_S* planes, int count);   // 任务分配

        void Cmd_DeployFightersToHoldingArea();  // 命令J15前往待战区
        void Cmd_DeployGuardsToHoldingArea();    // 命令护卫舰前往待战区

        // --- 执行 ---
        void ExecuteGuidance(PlaneState_S* planes, int count, double dt, double dtt);


    private:

        // 持久化存储每架飞机的目标位置 <PlaneID, {Lon, Lat}>
        std::map<int, std::pair<double, double>> _planeTargetPos;
        // 飞艇控制器，键(Key)是舰艇ID，值(Value)是该舰艇的专属控制器
        std::map<int, chuan_contrl> _shipControllers;
        // 飞控系统，键(Key)是飞机ID，值(Value)是该飞机的专属控制器
        std::map<int, FlightController> _flightControllers;

        // === 感知数据 ===
        FleetStatus _fleetInfo;
        bool _isScoutingOrdered;                 // 标记：是否已经下达过侦察命令
        bool _hasCalculatedHoldingAreas;         // 标记：是否已经计算过待战区
        bool _isFightersDeployedToHoldingArea;   // 标记：是否已经派遣过战斗机前往待战区
        bool _isGuardDeployedToHoldingArea;      // 标记：是否已经派遣过舰艇前往待战区

        // 发射控制变量
        double _launchTimer;           // 起飞计时器        
        double _logTimer;             // 日志计时器
        int _launchedFighterCount;     // 已起飞战斗机计数
        const int MAX_ASSEMBLY_SIZE = 4; // 集结区最大容量（比如保持4架）

        // === 战术规划数据 ===
        std::vector<HoldingArea> _fighterHoldingAreas; // 战斗机待战区
        std::vector<HoldingArea> _guardHoldingAreas;   // 舰艇待战区
        // 本帧战术分配结果
        std::map<int, UnitTask_E> _planeTaskStatus; // planeID -> 任务类型
        std::vector<ZoneAssignment> _zoneAssignments;
        std::set<int> _initializedTactics;
    };
}
