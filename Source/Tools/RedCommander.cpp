#include "RedCommander.h"
#include "ZHANSHU.h"
#include <iostream>
#include <cstring> 
#include <cmath>
#include <iomanip>
#include <limits> 
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <map>

namespace RedForce {

    // === 辅助函数：获取战术名称 ===
    std::string GetTacticName(TacticType_E t) {
        switch (t) {
        case TacticType_E::BaoWei_low_altitude: return "低空包围 (Low Surround)";
        case TacticType_E::BaoWei_high_altitude: return "高空包围 (High Surround)";
        case TacticType_E::YouDao: return "诱导 (YouDao)";
        case TacticType_E::QiShe: return "齐射 (QiShe)";
        default: return "未知";
        }
    }

    // === 构造函数 ===
    RedCommander::RedCommander()
        : _isScoutingOrdered(false),             //是否下过侦察命令
        _isFightersDeployedToHoldingArea(false), //是否战斗机已部署到待机区
        _isGuardDeployedToHoldingArea(false),    //是否防护机已部署到待机区
        _launchTimer(0.0),          // 起飞计时器初始化
        _launchedFighterCount(0),   // 已起飞战斗机计数初始化
        _logTimer(0.0)              // 日志计时器初始化
    {
        // 初始化随机种子
        srand((unsigned int)time(NULL));
    }

    // === 静态场景初始化函数 ===
    void RedCommander::InitRedUnits(PlaneState_S* planes, int count) {
        // 安全检查，防止数组越界
        if (count < 6) {
            std::cout << "[Error] Plane count is too small for this scenario!" << std::endl;
            return;
        }

        // 红方位置
        double baseLong = 116.200000;
        double baseLat = 21.577106;

        // [0] 山东舰 (航母)
        planes[0].init(LIMIT_FRIGATE1);                             //按舰艇1配置初始化设置
        planes[0]._planeID = 10010;                                 //编号（唯一）
        strcpy(planes[0]._pilot, "Red-Carrier");                    //别名（可选）
        planes[0]._longitude = baseLong;                            //初始经度
        planes[0]._latitude = baseLat;                              //初始纬度
        planes[0]._altitude = 0;                                    //初始高度
        planes[0]._yaw = 90;                                        //初始偏航角
        planes[0]._roll = 0;                                        //初始横滚角
        planes[0]._pitch = 0;                                       //初始俯仰角
        planes[0]._team = 1;                                        //设置团队（1-红方 2-蓝方）
        planes[0]._targetID = -1;                                   //目标设置（-1无目标）
        planes[0]._radarState = 1;                                  //雷达开关（0-关机 1-开机）
        planes[0]._throttle = 0.2;                                  //油门量
        planes[0]._equipmentType = AIRCRAFTCARRIER_SHANDONG;        //Tacview中显示的模型样式（此处设置的是航母山东舰）


        // [1] 护卫舰 (ID: 10011)
        // 055
        planes[1].init(LIMIT_FRIGATE1);                             //按舰艇1配置初始化设置
        planes[1]._planeID = 10011;                                 //编号（唯一）
        strcpy(planes[1]._pilot, "Red-Guard055");                   //别名（可选）
        planes[1]._longitude = baseLong + 0.5;                      //初始经度
        planes[1]._latitude = baseLat;                              //初始纬度
        planes[1]._altitude = 0;                                    //初始高度
        planes[1]._yaw = 90;                                        //初始偏航角
        planes[1]._roll = 0;                                        //初始横滚角
        planes[1]._pitch = 0;                                       //初始俯仰角
        planes[1]._team = 1;                                        //设置团队（1-红方 2-蓝方）
        planes[1]._targetID = -1;                                   //目标设置（-1无目标）
        planes[1]._radarState = 1;                                  //雷达开关（0-关机 1-开机）
        planes[1]._throttle = 0.2;                                  //油门量
        planes[1]._equipmentType = FRIGATE1;

        // [2-5] Su-33 舰载机群 [ID: 10012 - 10015] ---
        // 全部位于航母正上方，盘旋待命
        for (int i = 2; i < 6; i++)        //4 架飞机初始化
        {
            memset(planes + i, 0, sizeof(PlaneState_S));
            planes[i].init(MAV_RED_2);
            planes[i]._planeID = 10010 + i;

            // 位置：完全重叠在航母坐标上
            planes[i]._longitude = baseLong;
            planes[i]._latitude = baseLat;

            // 高度：分层排列，防止碰撞 (每架间隔 50米)
            planes[i]._altitude = 5000 - (i - 2) * 50;

            planes[i]._yaw = 90;
            planes[i]._roll = 0;
            planes[i]._pitch = 0;
            planes[i]._team = 1;
            planes[i]._targetID = -1;
            planes[i]._radarState = 1;
            planes[i]._throttle = 40;
            planes[i]._isAlive = -2; // 关键：悬停标记
            planes[i]._equipmentType = FIGHTER_J15;
            planes[i]._radar_range = 80000.0;   // 示例值：150公里
            planes[i]._radar_view_range = 80000.0;   // 示例值
            planes[i]._radar_hBeamWidth = 90;       // 水平波束宽度（度）
            planes[i]._radar_vBeamWidth = 60;        // 总垂直波束（若使用上下分置，可设为0或忽略）
            planes[i]._radar_vBeamWidth_upper = 90;        // 上半部分
            planes[i]._radar_vBeamWidth_below = 90;        // 下半部分
            planes[i]._radar_all_range = 80000.0;   // 全向最大探测距离
            planes[i]._radar_all_hBeamWidth = 360;
            planes[i]._radar_all_vBeamWidth = 180;
            strcpy(planes[i]._pilot, std::to_string(planes[i]._planeID).c_str());
        }
    }

    // === 1. 感知层 （航母收集信息） ===
    void RedCommander::GatherFleetInfo(PlaneState_S* planes, int count) {
        // 1. 清空上一帧的残留数据
        _fleetInfo.Reset();

        // 2. 遍历全场所有单位
        for (int i = 0; i < count; ++i) {
            PlaneState_S& p = planes[i];

            // 2.1 统计敌情 (Data Link)
            // 只要这个单位是我方的(Team 1) 且活着，它的雷达数据就有效
            if (p._team == 1 && p._isAlive > 0) {
                for (int k = 0; k < p._raderCaptureCount; ++k) {
                    int enemyID = p._raderCapturePlanesID[k];
                    if (enemyID > 0) {
                        // 放入集合，自动去重 (比如055和预警机都看到了同一个敌人，集合里只会有1个)
                        _fleetInfo.detectedEnemies.insert(enemyID);
                    }
                }
            }

            // 2.2 归纳我方单位状态
            if (p._team == 1) {
                // 构建简化信息
                UnitInfo info;
                info.index = i;
                info.id = p._planeID;
                info.isAlive = (p._isAlive > 0 || p._isAlive == -2); // 活着(包括悬停)
                info.missileCount = p._missileCount;
                info.lon = p._longitude;
                info.lat = p._latitude;
                // 1. 恢复任务状态
                if (_planeTaskStatus.count(info.id)) {
                    info.task = _planeTaskStatus[info.id];
                }
                else {
                    // 默认状态设置
                    info.task = (p._equipmentType == FIGHTER_J15) ? UnitTask_E::HoverOnCarrier : UnitTask_E::Idle;
                }

                // 2. 恢复目标坐标
                // 如果记事本里有这架飞机的目标记录，就读出来填进去
                if (_planeTargetPos.count(info.id)) {
                    info.targetLon = _planeTargetPos[info.id].first;
                    info.targetLat = _planeTargetPos[info.id].second;
                }
                // 分类入库
                switch (p._equipmentType) {
                case AIRCRAFTCARRIER_SHANDONG://航母
                    _fleetInfo.carrier = info;
                    break;
                case FIGHTER_J15://战斗机
                    _fleetInfo.fighters.push_back(info);
                    break;
                case FRIGATE1://护卫舰
                    _fleetInfo.guards.push_back(info);
                    break;
                default:
                    break;
                }
            }
        }
    }

    // === 1.5 舰载机起飞管理循环 ===
    void RedCommander::ManageCarrierLaunchCycle(double dt, PlaneState_S* planes) {
        _launchTimer += dt;
        const double LAUNCH_INTERVAL = 5.0; // 每5秒起飞一架

        if (_launchTimer < LAUNCH_INTERVAL) return;

        // === 战斗机起飞补给 (动态补给集结区) ===
        // 统计集结区现状 (在集结区 + 去集结区)
        int fightersInAir = 0;
        for (const auto& f : _fleetInfo.fighters)
        {
            if (f.task == UnitTask_E::InAssemblyArea || f.task == UnitTask_E::GoToAssemblyArea)
            {
                fightersInAir++;
            }
        }

        // 如果集结区缺人，且甲板上还有飞机
        if (fightersInAir < MAX_ASSEMBLY_SIZE) {
            for (auto& fighter : _fleetInfo.fighters) {
                if (fighter.task == UnitTask_E::HoverOnCarrier && fighter.missileCount >= 4) {

                    // 下令起飞
                    fighter.task = UnitTask_E::GoToAssemblyArea;
                    // 必须更新持久化状态！
                    _planeTaskStatus[fighter.id] = UnitTask_E::GoToAssemblyArea;

                    // 计算新的集结区位置
                    double assemblyBaseLon = planes[_fleetInfo.carrier.index]._longitude;
                    double assemblyBaseLat = planes[_fleetInfo.carrier.index]._latitude;

                    // 设置位置
                    fighter.targetLon = assemblyBaseLon + 0.01;
                    fighter.targetLat = assemblyBaseLat;
                    _planeTargetPos[fighter.id] = { fighter.targetLon, fighter.targetLat };

                    _launchTimer = 0;
                    break; // 一次只飞一架
                }
            }
        }
    }

    // === 2. 决策层 (大脑) ===
    void RedCommander::MakeStrategicDecisions(PlaneState_S* planes, int count) {
        if (_fleetInfo.detectedEnemies.empty()) return;
        if (_hasCalculatedHoldingAreas == false)
        {
            // 1. 计算待战区
            CalculateHoldingAreas();
        }
        // 2. 下达前往待战区命令 (一旦计算好就下达)
        Cmd_DeployFightersToHoldingArea();
        Cmd_DeployGuardsToHoldingArea();
        // 3. 战术分配与执行
        MakeTacticalDecisions(planes, count);
    }

    // === 3. 指令集 (设置任务目标) ===
    // 计算待战区
    void RedCommander::CalculateHoldingAreas() {
        if (_fleetInfo.detectedEnemies.empty()) return;

        // 计算所有敌人位置的平均值
        double sumLon = 0, sumLat = 0;
        int num = 0;
        for (int eid : _fleetInfo.detectedEnemies) {
            const PlaneState_S* e = GetPlaneState(eid);
            if (e && e->_isAlive > 0) {
                sumLon += e->_longitude;
                sumLat += e->_latitude;
                num++;
            }
        }
        if (num == 0) return;

        double centerLon = sumLon / num;
        double centerLat = sumLat / num;
        double carrierLon = _fleetInfo.carrier.lon;
        double carrierLat = _fleetInfo.carrier.lat;

        // 计算向量：敌人 -> 航母
        double vecLon = carrierLon - centerLon;
        double vecLat = carrierLat - centerLat;
        double len = sqrt(vecLon * vecLon + vecLat * vecLat);

        // 归一化
        double dirLon = (len > 0) ? (vecLon / len) : 1.0;
        double dirLat = (len > 0) ? (vecLat / len) : 0.0;

        // 设定待战区位置：在敌人和航母连线上，距离敌人一定距离
        // 4架飞机比较集中，我们只设 1 个飞机待战区，1 个护卫舰待战区
        _fighterHoldingAreas.clear();
        _guardHoldingAreas.clear();

        // 飞机待战区：距离敌人 2.0 度
        HoldingArea fa;
        fa.lon = centerLon + dirLon * 2.0;
        fa.lat = centerLat + dirLat * 2.0;
        fa.alt = 8000;
        _fighterHoldingAreas.push_back(fa);

        // 护卫舰待战区：距离敌人 1.5 度
        HoldingArea ga;
        ga.lon = centerLon + dirLon * 1.5;
        ga.lat = centerLat + dirLat * 1.5;
        ga.alt = 0;
        _guardHoldingAreas.push_back(ga);

        _hasCalculatedHoldingAreas = true;
    }

    // 派遣J15前往待战区
    void RedCommander::Cmd_DeployFightersToHoldingArea() {
        if (_fighterHoldingAreas.empty()) return;

        // 统计集结区有多少可用飞机 (只选已经到达 InAssemblyArea 的)
        int availableCount = 0;
        for (auto& f : _fleetInfo.fighters) {
            if (f.task == UnitTask_E::InAssemblyArea) availableCount++;
        }

        if (availableCount == 0) return;

        int deployedCount = 0;
        // 遍历待战区进行分配
        for (size_t i = 0; i < _fighterHoldingAreas.size(); ++i) {
            const auto& targetArea = _fighterHoldingAreas[i];
            int assignedToThisArea = 0;

            for (auto& fighter : _fleetInfo.fighters) {
                if (fighter.task == UnitTask_E::InAssemblyArea) {

                    // 1. 设置任务
                    fighter.task = UnitTask_E::GoToHoldingArea;
                    _planeTaskStatus[fighter.id] = UnitTask_E::GoToHoldingArea; // 存状态

                    // 2. 设置当前帧的坐标
                    fighter.targetLon = targetArea.lon;
                    fighter.targetLat = targetArea.lat;

                    // 3.存入持久化 Map 
                    _planeTargetPos[fighter.id] = { targetArea.lon, targetArea.lat };

                    assignedToThisArea++;
                    deployedCount++;
                }
            }
            if (deployedCount >= availableCount) break;
        }
    }

    // 派遣护卫舰前往待战区
    void RedCommander::Cmd_DeployGuardsToHoldingArea() {
        if (_guardHoldingAreas.empty()) return;
        auto& targetArea = _guardHoldingAreas[0];

        for (auto& g : _fleetInfo.guards) {
            if (g.task == UnitTask_E::Idle) {
                g.task = UnitTask_E::GoToHoldingArea;
                _planeTaskStatus[g.id] = UnitTask_E::GoToHoldingArea;
                g.targetLon = targetArea.lon;
                g.targetLat = targetArea.lat;
                _planeTargetPos[g.id] = { targetArea.lon, targetArea.lat };
            }
        }
    }

    // 战术决策：遍历4个待战区，按“包围/诱导/模版”规则在相邻待战区间挑选飞机
    void RedCommander::MakeTacticalDecisions(PlaneState_S* planes, int count) {

        // 1. 清理已失效的任务 (目标死亡 或 执行者全部阵亡)
        for (auto it = _zoneAssignments.begin(); it != _zoneAssignments.end(); ) {
            bool shouldRTB = false;
            std::string reason = "";

            // --- A. 检查目标状态 ---
            const PlaneState_S* target = GetPlaneState(it->targetId);
            bool targetDead = (!target || target->_isAlive <= 0);
            bool targetLost = (_fleetInfo.detectedEnemies.find(it->targetId) == _fleetInfo.detectedEnemies.end());

            if (targetDead) { shouldRTB = true; reason = "Target Destroyed"; }
            else if (targetLost) { shouldRTB = true; reason = "Target Lost"; }

            //  B. 检查飞机状态 (弹药 / 返航 / 存活) ---
            if (!shouldRTB) {
                int aliveCount = 0; // 统计幸存者

                for (int pid : it->planeIds) {
                    bool isThisPlaneAlive = false;

                    // 在舰队列表中查找该飞机
                    for (auto& f : _fleetInfo.fighters) {
                        if (f.id == pid) {
                            // 1. 检查存活
                            if (f.isAlive) {
                                isThisPlaneAlive = true;
                            }

                            // 2. 检查是否已经在返航 (如果是，视为任务结束)
                            if (f.task == UnitTask_E::ReturnToBase || f.task == UnitTask_E::HoverOnCarrier) {
                                shouldRTB = true; reason = "Planes Returning";
                            }

                            // 3. 检查弹药 (活着才查)
                            if (f.isAlive && f.missileCount <= 0) {
                                shouldRTB = true; reason = "Ammo Depleted";
                            }
                            break;
                        }
                    }

                    if (isThisPlaneAlive) aliveCount++;
                }

                // 如果小队全军覆没，清除任务
                if (aliveCount == 0) {
                    shouldRTB = true;
                    reason = "Squadron Eliminated"; // 全员阵亡
                }
            }

            // --- C. 执行返航决策 ---
            if (shouldRTB) {
                std::cout << "[Red Command] Task Finished (" << reason << "). Clearing assignment." << std::endl;

                for (int pid : it->planeIds) {
                    // 找到对应的飞机信息
                    for (auto& f : _fleetInfo.fighters) {
                        if (f.id == pid) {
                            // 强制所有队员返航 (除非已经到了集结区或甲板)
                            if (f.task != UnitTask_E::HoverOnCarrier &&
                                f.task != UnitTask_E::ReturnToBase &&
                                f.task != UnitTask_E::GoToAssemblyArea) { // 避免打断已经重置的飞机

                                f.task = UnitTask_E::ReturnToBase;
                                _planeTaskStatus[pid] = UnitTask_E::ReturnToBase;
                                std::cout << "   -> Fighter " << pid << " Returning To Base (RTB)." << std::endl;
                            }
                        }
                    }
                }

                // 任务已结束，从列表中移除
                it = _zoneAssignments.erase(it);
            }
            else {
                ++it;
            }
        }

        // 2. 如果当前有正在进行的飞机攻击任务，就不分配新任务 (一次只打一个，专注)
        bool isAttacking = false;
        for (const auto& assign : _zoneAssignments) {
            if (assign.tactic != TacticType_E::QiShe) { // 齐射不占用飞机，不算
                isAttacking = true;
                break;
            }
        }
        if (isAttacking) return;

        // 3. 确定攻击目标 (优先级：20011护卫舰 -> 20010航母 -> 飞机)
        // 6v6 场景下，蓝方ID: 20010(Carrier), 20011(Guard), 20012-20015(Fighters)
        int targetId = -1;
        int priorities[] = { 20010, 20011, 20012, 20013, 20014, 20015 };

        for (int pid : priorities) {
            const PlaneState_S* p = GetPlaneState(pid);
            if (p && p->_isAlive > 0 && _fleetInfo.detectedEnemies.count(pid)) {
                targetId = pid;
                break;
            }
        }
        if (targetId == -1) return; // 没发现有价值目标

        // 4. 统计待战区有多少飞机可用
        if (_fighterHoldingAreas.empty()) return;
        std::vector<int> availablePlanes;
        // 直接遍历所有飞机，看谁在待战区盘旋
        for (auto& f : _fleetInfo.fighters) {
            // 判断条件：状态是 InHoldingArea 且没有正在攻击
            if (f.task == UnitTask_E::InHoldingArea) {
                availablePlanes.push_back(f.id);
            }
        }

        // 5. 随机选择战术 (前提：至少有2架飞机)
        if (availablePlanes.size() >= 2) {
            // 随机数 0~2
            // 1: BaoWei_Low (低空包围)
            // 2: BaoWei_High (高空包围)
            // 3: YouDao (诱导, 至少3架)

            std::vector<TacticType_E> options;
            options.push_back(TacticType_E::BaoWei_low_altitude);
            options.push_back(TacticType_E::BaoWei_high_altitude);
            if (availablePlanes.size() >= 4) {
                options.push_back(TacticType_E::YouDao);
            }

            int rnd = rand() % options.size();
            TacticType_E selectedTactic = options[rnd];

            // 6. 分配所有可用飞机 (All In)
            ZoneAssignment assign;
            assign.zoneId = 0;
            assign.targetId = targetId;
            assign.tactic = selectedTactic;
            assign.planeIds = availablePlanes; // 全部投入

            // 设置飞机状态
            for (int pid : availablePlanes) {
                _planeTaskStatus[pid] = UnitTask_E::Attack;
                for (auto& f : _fleetInfo.fighters) if (f.id == pid) f.task = UnitTask_E::Attack;

                // 从待战区记录中移除
                _fighterHoldingAreas[0].presentUnitIds.erase(pid);
            }

            _zoneAssignments.push_back(assign);
            std::cout << "[Red Command] Tactic ACTIVATED!" << std::endl;
            std::cout << "   Target: " << targetId << std::endl;
            std::cout << "   Tactic: " << GetTacticName(selectedTactic) << std::endl;
            std::cout << "   Planes: ";
            for (int pid : availablePlanes) std::cout << pid << " ";
            std::cout << std::endl;
        }

        // 7. 舰艇齐射 (独立判断，不占用飞机)
        // 简单逻辑：只要护卫舰到了待战区，就对目标开火
        bool guardReady = false;
        for (auto& g : _fleetInfo.guards) {
            if (g.task == UnitTask_E::InHoldingArea) guardReady = true;
        }

        // 检查是否已经对该目标齐射
        bool alreadyQiShe = false;
        for (auto& a : _zoneAssignments) {
            if (a.targetId == targetId && a.tactic == TacticType_E::QiShe) alreadyQiShe = true;
        }

        if (guardReady && !alreadyQiShe) {
            ZoneAssignment assign;
            assign.zoneId = -1;
            assign.targetId = targetId;
            assign.tactic = TacticType_E::QiShe;
            // 放入护卫舰ID
            for (auto& g : _fleetInfo.guards) assign.planeIds.push_back(g.id);

            _zoneAssignments.push_back(assign);
            std::cout << "[Tactic] Guard QiShe initiated." << std::endl;
        }
    }

    // === 4. 执行层 (底层/中层逻辑) ===
    void RedCommander::ExecuteGuidance(PlaneState_S* planes, int count, double dt, double dtt) {
        PlaneState_S* carrier = &planes[_fleetInfo.carrier.index];
        if (!carrier) return;

        // 战术失效保护 (Watchdog) ZHANSHU.cpp 内部状态冲突时，导致飞机处于 Attack 状态却无人指挥
        for (auto& f : _fleetInfo.fighters) {
            // 只检查处于攻击状态的飞机
            if (f.task == UnitTask_E::Attack) {
                bool hasValidAssignment = false;

                // 遍历当前所有任务单，看这架飞机是否在名单里
                for (const auto& assign : _zoneAssignments) {
                    for (int pid : assign.planeIds) {
                        if (pid == f.id) {
                            hasValidAssignment = true;
                            break;
                        }
                    }
                    if (hasValidAssignment) break;
                }

                // 如果飞机以为自己在攻击，但指挥官手里没有它的任务单（成了孤儿）
                // 或者虽然有任务单，但底层战术算崩了（表现为长时间没有发射且没有机动）
                // 这里我们简单粗暴处理：没有任务单就强制返航
                if (!hasValidAssignment) {
                    PlaneState_S* p = &planes[f.index];

                    // 强制修改状态为返航
                    f.task = UnitTask_E::ReturnToBase;
                    _planeTaskStatus[f.id] = UnitTask_E::ReturnToBase;

                    // 重置底层状态，防止干扰
                    p->_targetID = -1;
                    p->_isShoot = false;
                    p->_radarState = 1; // 恢复搜索雷达

                    std::cout << "[Red Safety] Watchdog triggered! Fighter " << f.id << " lost link to tactic. Forcing RTB." << std::endl;
                }
            }
        }
        // 1. 飞机任务
        for (auto& f : _fleetInfo.fighters) {
            PlaneState_S* p = &planes[f.index];
            FlightController& fc = _flightControllers[f.id];
            // 集结区跟随航母移动
            if (f.task == UnitTask_E::GoToAssemblyArea || f.task == UnitTask_E::InAssemblyArea) {
                double offsetLon = 0.05;
                f.targetLon = carrier->_longitude + offsetLon;
                f.targetLat = carrier->_latitude; // 纬度保持一致

                // 同步更新持久化数据，防止状态跳变
                _planeTargetPos[f.id] = { f.targetLon, f.targetLat };
            }
            switch (f.task) {
            case UnitTask_E::HoverOnCarrier:
                p->_isAlive = -2; // 保持悬停状态
                p->_throttle = 0;
                // 保持在航母甲板位置
                SetPlanePosition(f.id, carrier->_longitude, carrier->_latitude,
                    5000 - (f.index - 2) * 50,
                    carrier->_velocity_north, carrier->_velocity_east);
                p->_longitude = carrier->_longitude;
                p->_latitude = carrier->_latitude;

                // 如果飞机停在甲板上，且弹药不满，立即补满
                if (p->_missileCount < 4) { // 假设满弹量是 4 枚
                    p->_missileCount = 4;   // 强行补满弹药
                    p->_radarState = 1;     // 雷达恢复为搜索状态
                    p->_targetID = -1;      // 清除旧的锁定目标
                    p->_isShoot = false;    // 重置发射状态
                }
                break;

            case UnitTask_E::GoToAssemblyArea: // 起飞
                if (p->_isAlive == -2) p->_isAlive = 1;
                if (pow(p->_longitude - f.targetLon, 2) + pow(p->_latitude - f.targetLat, 2) < 0.0001) {
                    f.task = UnitTask_E::InAssemblyArea;
                    _planeTaskStatus[f.id] = UnitTask_E::InAssemblyArea;
                    std::cout << "[Red] Fighter " << f.id << " >>> Arrived at Assembly Area." << std::endl;
                }
                else {
                    fc.run_control(*p, f.targetLon, f.targetLat, 6000, 400.0, dt);
                }
                break;

            case UnitTask_E::InAssemblyArea: // 集结
                fc.Loiter(p, f.targetLon, f.targetLat, 6000, 2000, 400);
                break;

            case UnitTask_E::GoToHoldingArea: // 去待战区
                if (p->_isAlive == -2) p->_isAlive = 1;
                if (pow(p->_longitude - f.targetLon, 2) + pow(p->_latitude - f.targetLat, 2) < 0.01) {
                    f.task = UnitTask_E::InHoldingArea;
                    _planeTaskStatus[f.id] = UnitTask_E::InHoldingArea;
                    if (!_fighterHoldingAreas.empty()) _fighterHoldingAreas[0].presentUnitIds.insert(f.id);
                    std::cout << "[Red] Fighter " << f.id << " >>> Arrived at Holding Area (Ready to fight)." << std::endl;
                }
                else {
                    fc.run_control(*p, f.targetLon, f.targetLat, 6000, 800, dt);
                }
                break;

            case UnitTask_E::InHoldingArea: // 待战
                fc.Loiter(p, f.targetLon, f.targetLat, 8000, 2000, 400);
                break;

            case UnitTask_E::Attack:
                // 攻击逻辑由下面的战术执行块处理，这里只负责检查是否需要返航
                if (p->_missileCount <= 0) {
                    f.task = UnitTask_E::ReturnToBase;
                    _planeTaskStatus[f.id] = UnitTask_E::ReturnToBase;
                }
                break;

            case UnitTask_E::ReturnToBase:
                f.targetLon = carrier->_longitude;
                f.targetLat = carrier->_latitude;
                if (pow(p->_longitude - f.targetLon, 2) + pow(p->_latitude - f.targetLat, 2) < 0.0001) {
                    f.task = UnitTask_E::HoverOnCarrier;
                    _planeTaskStatus[f.id] = UnitTask_E::HoverOnCarrier;
                }
                else {
                    fc.run_control(*p, f.targetLon, f.targetLat, 6000, 400, dt);
                }
                break;
            }
        }

        // 2. 舰艇任务 (只保留基本移动)
        for (auto& g : _fleetInfo.guards) {
            PlaneState_S* p = &planes[g.index];
            chuan_contrl& ctrl = _shipControllers[g.id];

            if (g.task == UnitTask_E::GoToHoldingArea) {
                ctrl.chuan_go_position(*p, 20, g.targetLon, g.targetLat);
                if (pow(p->_longitude - g.targetLon, 2) + pow(p->_latitude - g.targetLat, 2) < 0.01) {
                    g.task = UnitTask_E::InHoldingArea;
                    _planeTaskStatus[g.id] = UnitTask_E::InHoldingArea;
                    std::cout << "[Red] PlaneState " << g.id << " >>> Arrived at Holding Area (Ready to fight)." << std::endl;
                }
            }
            else if (g.task == UnitTask_E::InHoldingArea) {
                ctrl.chuan_go_position(*p, 5, g.targetLon, g.targetLat); // 慢速维持
            }
        }

        // 3. 战术执行 (调用 ZHANSHU.h 中的函数)
        // Per-target tactic instances (no shared global state)
        static std::unordered_map<int, ZhanShu::BaoWeiTactic> s_baoweiByTarget;

        for (const auto& assign : _zoneAssignments) {
            const PlaneState_S* target = GetPlaneState(assign.targetId);
            if (!target || target->_isAlive <= 0) continue;

            std::vector<int> myPlanes = assign.planeIds;
            int carrierId = 10010;
            bool support = false;
            std::vector<int> emptySupport;

            switch (assign.tactic) {
            case TacticType_E::BaoWei_low_altitude:
            {
                auto& tactic = s_baoweiByTarget[assign.targetId];
                tactic.zhanshu_muban_yule_baowei(planes, count, myPlanes, assign.targetId,
                    180.0, 0.6, 1.2,
                    carrier->_longitude, carrier->_latitude,
                    (int)myPlanes.size(), 60000.0, 10000.0, 5000.0,
                    emptySupport, support);
                break;
            }
            case TacticType_E::BaoWei_high_altitude:
            {
                auto& tactic = s_baoweiByTarget[assign.targetId];
                tactic.zhanshu_muban_yule_baowei(planes, count, myPlanes, assign.targetId,
                    50.0, 1.0, 1.05,
                    carrier->_longitude, carrier->_latitude,
                    (int)myPlanes.size(), 120000.0, 12000.0, 12000.0,
                    emptySupport, support);
                break;
            }
            case TacticType_E::YouDao:
                ZhanShu::zhanshu_moban_luo_youdao(planes, myPlanes, assign.targetId, carrierId, dtt);
                break;
            case TacticType_E::QiShe:
            {
                int tIdx = -1;
                for (int i = 0; i < count; ++i) if (planes[i]._planeID == assign.targetId) tIdx = i;
                if (tIdx != -1) ZhanShu::chuan_qishe(3, dtt, planes, tIdx, 20, myPlanes);
                break;
            }
            }
        }
    }

    // === 主循环 Update ===
    void RedCommander::Update(double dt, PlaneState_S* planes, int count, double dtt) {
        // 1. 感知：
        GatherFleetInfo(planes, count);

        // 1.5 舰载机起飞管理循环
        ManageCarrierLaunchCycle(dt, planes);

        // 2. 决策：(发号施令)
        MakeStrategicDecisions(planes, count);

        // 3. 执行：(底层干活)
        ExecuteGuidance(planes, count, dt, dtt);
    }
}
