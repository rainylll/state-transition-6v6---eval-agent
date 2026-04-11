#pragma once
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <map>

#include "../Tools/PlatformCompat.h"
#include "../CombatSimulation/UnitDefine.h"
#include "../TacView/TacViewOutput.h"
#include "../TacView/wgua.h"
#include "../Tools/joystick_gamepad.h"

using namespace std;
using namespace TacView;
using namespace Eigen;
using namespace CombatSimulation;

#define Max_Friend_Count            1                           //!<最大友方数量
#define Max_Plane_Count             128                         //!<最大飞机数量
#define Max_Missile_Count           6                           //!<每架飞机最大挂载导弹数量
#define Max_Warn_Count              16                          //!<最大警告数量

#define YAW_BASE_SPEED              0.2
#define PITCH_BASE_SPEED            0.2
#define ROLL_BASE_SPEED             0.2
#define DEFAULT_THROTTLE            20

#pragma pack(1)

typedef enum
{
    Close,                                                      //!< 雷达关闭
    Scan,                                                       //!< 雷达扫描
    Track,                                                      //!< 雷达跟踪
    Fixed                                                       //!< 雷达随机头固定角度
} RadarState_E;

typedef enum
{
    RM_Normal,                                                  //!< 正常扫描模式
    HGSynergy,                                                  //!< 双机HG模式
    HGSingle,                                                   //!< 单机HG模式
    ESMSynergy,                                                 //!< ESM(电子支援措施)协同模式
    RelayGuidance                                               //!< 接力制导
} RadarMode_E;

typedef enum
{
    GM_Normal,                                                  //!< 正常制导
    Relay                                                       //!< 接力制导
} GuidanceMode_E;

typedef struct
{
    int         _planeID;
    int         _targetID;
    int         _missileID;
} RelayGuidance_S;

typedef struct
{
    bool        _isOpen;
    double      _radarLength;
    double      _overlapHAngle;
    double      _overlapVAngle;
    double      _overlapLength;
    double      _originX;
    double      _originY;
    double      _originZ;
} HGSynergy_S;

typedef struct
{
    bool    _isOpen;
    bool    _isUsable;
    bool    _isMaster;
    int     _friendId;
    int     _enemyId;
} ESMSynergy_S;

typedef enum
{
    RECV_RADAR_WAVES = 1,                                       //!< 接收到雷达波
} WarnType_E;

typedef struct
{
    int             _targetID;                                  //!< 目标飞机ID
    double          _tarLongitude;                              //!< 目标坐标，经度，单位：度
    double          _tarLatitude;                               //!< 目标坐标，纬度，单位：度
    double          _tarAltitude;                               //!< 目标坐标，高度，单位：米
    double          _time;                                      //!< 加入告警的时间
} WarnState_S;

typedef struct
{
    unsigned int    _missileCount;                              //!< 导弹数量
    double          _heightAtLaunch;                            //!< 导弹发射时与目标的高度
    double          _distanceAtLaunch;                          //!< 导弹发射时与目标的距离
    double          _successRateLowerLimit;                     //!< 反导成功率下限
    double          _successRateUpperLimit;                     //!< 反导成功率上限
} AntiMissileParam_S;

typedef struct CIWS_S
{
    double          _dt;                                        //!< 显示的时间
    double          _maxRange;                                  //!< 最大射程
    double          _yaw;                                       //!< 射击偏航角度
    double          _maxYaw;                                    //!< 最大覆盖水平宽度（角度）
    double          _maxPitch;                                  //!< 最大覆盖垂直高度（角度）
    double          _showCount;                                 //!< 显示数量
    double          _radius;                                    //!< 单个爆破效果的半径（米）

    CIWS_S()
    {
        _dt = 1.0;
        _maxRange = 3000;
        _yaw = 0;
        _maxYaw = 10;
        _maxPitch = 70;
        _showCount = 10;
        _radius = 3;
    }
} CIWS_S;

typedef struct
{
    bool            _isHit;                                     //!< 预设导弹是否命中目标
    int             _missileID;                                 //!< 导弹ID
    int             _targetID;                                  //!< 目标飞机ID
    int             _missile_live;                              //!< 导弹状态 （1）-运行中 （0）-未发射 （-1）-失去引导 （-2）-未命中 （-3）-命中
    int             _radar_state;                               //!< 末制导雷达状态 0-雷达未捕获目标  1-末制导雷达捕获目标
    GuidanceMode_E  _ruidanceMode;                              //!< 制导模式
    double          _longitude;                                 //!< 坐标，经度，单位：度
    double          _latitude;                                  //!< 坐标，纬度，单位：度
    double          _altitude;                                  //!< 坐标，高度，单位：米

    double          _heightAtLaunch;                            //!< 发射时与目标的高度
    double          _distanceAtLaunch;                          //!< 发射时与目标的距离
} MissileState_S;

typedef struct PlaneState_S
{
    char            _limitIndex;                                //!< 功能限制索引 -1为无限制
    char            _isAlive;                                   //!< 是否存活 0-被击毁 1-存活 -1-坠毁 -2-悬停
    bool            _isShoot;                                   //!< 发射导弹
    bool            _isWaypointMode;                            //!< 航路点飞行模式
    bool			_isRadarModeChange;
    bool            _isSpeedAdjust;                             //!< 是否调节过速度

    EquipmentType_E _equipmentType;                             //!< 装备类型
    int             _radarState;                                //!< 雷达状态 0-关机 1-搜索 2-锁定
    int             _raderCaptureCount;                         //!< 雷达捕获敌机数量

    int             _planeID;                                   //!< 飞机ID
    int             _team;                                      //!< 团队 1-红方 2-蓝方

    int             _targetID;                                  //!< 攻击的目标ID
    int             _missileCount;                              //!< 导弹数量

    int             _recvRadarWarnCount;                        //!< 警告数量

    int             _friendCount;                               //!< 友方数量
    float           _overload;                                  //!< 当前过载
    RadarMode_E     _radarMode;                                 //!< 当前雷达模式

    double          _battleTime;                                //!< 战场时间
    double          _longitude;                                 //!< 坐标，经度，单位：度
    double          _latitude;                                  //!< 坐标，纬度，单位：度
    double          _altitude;                                  //!< 坐标，高度，单位：米

    double          _roll;                                      //!< 坐标，滚转角，单位：度
    double          _pitch;                                     //!< 坐标，俯仰角，单位：度
    double          _yaw;                                       //!< 坐标，偏航角，单位：度，原始点：正北，正方向：顺时针

    double          _roll_ctrl;                                 //!< 滚转角控制（范围 -1 至 1）
    double          _pitch_ctrl;                                //!< 俯仰角控制（范围 -1 至 1）
    double          _yaw_ctrl;                                  //!< 偏航角控制（范围 -1 至 1）
    double          _throttle;                                  //!< 油门控制（范围 0 至 100）

    double          _velocity_north;                            //!< 速度，北向，单位：米/秒
    double          _velocity_east;                             //!< 速度，东向，单位：米/秒
    double          _velocity_downward;                         //!< 速度，地面方向，单位：米/秒

    double          TAS;                                        //!< 真空速

    double          _radar_range;                               //!< 雷达扫描距离，单位 米
    double          _radar_view_range;                          //!< 雷达视觉范围，单位：米
    double          _radar_hBeamWidth;                          //!< 雷达水平波束宽度（角度）
    double          _radar_vBeamWidth;                          //!< 雷达波束垂直高度（角度）
    double          _radar_vBeamWidth_upper;                    //!< 雷达波束垂直高度（角度）（上）
    double          _radar_vBeamWidth_below;                    //!< 雷达波束垂直高度（角度）（下）
    double          _radar_all_hBeamWidth;                      //!< 雷达扫描全水平宽度（角度）
    double          _radar_all_vBeamWidth;                      //!< 雷达扫描全垂直宽度（角度）
    double          _radar_all_range;                           //!< 雷达全探测距离，单位 米
    double          _radar_azimuth;                             //!< 当前雷达扫描的偏航角度
    double          _radar_elevation;                           //!< 当前雷达扫描的俯仰角度
    double          _radar_all_azimuth;                         //!< 当前雷达扫描的全偏航角度
    double          _radar_all_elevation;                       //!< 当前雷达扫描的全俯仰角度

    HGSynergy_S     _HGSynergy;                                 //!< 双机HG使用的数据
    ESMSynergy_S    _ESMSynergy;                                //!< ESM协同模式使用的数据
    RelayGuidance_S _relayGuidance;                             //!< 接力制导
    CIWS_S          _CIWS;                                      //!< 近防炮

    int             _friendList[Max_Friend_Count];              //!< 友方ID列表
    int             _raderCapturePlanesID[Max_Plane_Count];     //!< 雷达捕获到飞机ID列表

    MissileState_S  _missileState[Max_Missile_Count];           //!< 导弹状态
    WarnState_S     _recvRadarWarnState[Max_Warn_Count];        //!< 接收到雷达波警告状态

    char            _pilot[64];

    void init(char limitIndex = 0);
} PlaneState_S;

//目标状态信息输入接口 (东大团队提供)
typedef struct
{
    int    infoType;                        // 0距离缺维信息   1全维信息粗  2全维信息精
    int    tID;                             // 敌方飞机的编号
    double tVE;                             // 敌方飞机的东向速度
    double tVN;                             // 敌方飞机的北向速度
    double tVS;                             // 敌方飞机的地向速度
    double tYaw;                            // 敌方飞机航迹方位角（偏航角）度
    double tPitch;                          // 敌方飞机航迹倾斜角（俯仰角）度
    double tRoll;                           // 敌方飞机航迹倾斜角（滚转角）度

    //大地坐标系
    double tLongitude;                      // 敌方飞机的经度
    double tLatitude;                       // 敌方飞机的纬度
    double tAltitutde;                      // 敌方飞机海拔

    //极坐标系 
    double tmAzi;                           // 敌我方位角
    double tmPitch;                         // 敌我俯仰角
    double tmDict;                          // 敌我距离
} TargetInfo;

// 模式显示 二级菜单显示 (东大团队提供)
typedef struct
{
    int     DistType;       // 距离类型 1-TR, 2-DOR, 3-MSR, 4-DR, 5-MOR, 6-MAR
    int     Mode;           // 0-搜索 1-中制导 2-攻击 3-躲避 4-迂回
    int     ModeState;      // 0-初次搜索 1-重搜索 2-攻击占位 3-攻击 4-向左偏置 5-向右偏置 6-躲避一  7-躲避二  8-躲避三  9-向左迂回 10-向右迂回
    double  DistShow;       // 显示实际距离
} ShowDist;

// 收发分治
typedef struct
{
    vector<PlaneState_S*>   friendPlanes;
    vector<PlaneState_S*>   targetPlanes;
} FriendRaderCapture_S;

#pragma pack()

extern "C++"
{
    /**
    *   @brief          获得TargetInfo数据
    *   @param[in]      selfPlane       控制机的PlaneState_S指针
    *   @param[in]      inRadarMode     雷达模式
    */
    SEA_AIR_API vector<TargetInfo> GetTargetInfo(PlaneState_S* selfPlane, RadarMode_E inRadarMode);
}

extern "C"
{
    /**
    *   @brief          单机初始化环境
    *   @param[in]      acmiPath            ACMI文件保存路径
    *   @param[in]      planes              飞机初始化数据
    *   @param[in]      size                飞机数量
    *   @retval         bool
    */
    SEA_AIR_API bool InitEnv(const char* acmiPath, PlaneState_S* planes, uint32_t size);

    /**
    *   @brief          单步解算
    *   @param[in]      dt                  单步时间间隔 单位：秒
    *   @param[in]      planes              飞机数据
    *   @retval         bool
    */
    SEA_AIR_API bool EnvStep(double dt, PlaneState_S* planes);

    /**
    *   @brief          雷达独立单步解算
    *   @param[in]      speed               扫描速度（几倍速度）
    *   @retval         bool
    */
    SEA_AIR_API void RadarStep(int speed);

    /**
    *   @brief          添加战场边界
    *   @param[in]      num                 边界编号
    *   @param[in]      baseLongitude       基点经度（左上角）
    *   @param[in]      baseLatitude        基点纬度（左上角）
    *   @param[in]      baseAltitude        基点高度（左上角）
    *   @param[in]      xLen                长度(米)
    *   @param[in]      yLen                宽度(米)
    *   @param[in]      zLen                高度(米)
    *   @param[in]      color               颜色
    */
    SEA_AIR_API void AddBoundary(int num, double baseLongitude, double baseLatitude, double baseAltitude,
        double xLen, double yLen, double zLen, Color_E color);

    /**
    *   @brief          指定点添加球状体
    */
    SEA_AIR_API int AddCircle(double longitude, double latitude, double altitude, int radius, Color_E color);

    /**
    *   @brief          移除球状体
    */
    SEA_AIR_API void RemoveCircle(int cid);

    /**
    *   @brief          绘制十字格栅
    */
    SEA_AIR_API void DrawCrossGrid(double baseLongitude, double baseLatitude, double baseAltitude, int childSize, int hNum, int vNum, Color_E color);

    /**
    *   @brief          几秒后将指定物体随机移动到十字格栅的某个位置上
    */
    SEA_AIR_API void RadomMoveCrossGrid(PlaneState_S* planes, int second);

    /**
    *   @brief          设置航路点规划
    *   @param[in]      PathTomonitor_S&    路径规划结构体
    */
    void SetPathPlan(const PathTomonitor_S& pathTomonitor);

    /**
    *   @brief          获得飞机当前状态
    *   @param[in]      in_id       飞机ID
    *   @retval         const PlaneState_S*
    */
    SEA_AIR_API const PlaneState_S* GetPlaneState(const int in_id);

    /**
    *   @brief          判断指定HG雷达协同模式是否可用
    *   @param[in]      selfPlane   控制机的PlaneState_S指针
    *   @retval         bool        是否可用
    */
    SEA_AIR_API bool IsHGSynergy(PlaneState_S* selfPlane);

    /**
    *   @brief          判断指定ESM雷达协同模式是否可用
    *   @param[in]      selfPlane       控制机的PlaneState_S指针
    *   @param[in]      enemyPlaneID    敌机ID
    *   @retval         bool            是否可用
    */
    SEA_AIR_API bool IsESMSynergy(PlaneState_S* selfPlane, int enemyPlaneID);

    /**
    *   @brief          设置雷达模式
    *   @param[in]      selfPlane   控制机的PlaneState_S指针
    *   @param[in]      radarMode   雷达模式
    *   @param[in]      radarState  雷达状态
    */
    SEA_AIR_API void SetRadarMode(PlaneState_S* selfPlane, RadarMode_E radarMode, RadarState_E radarState = RadarState_E::Scan);

    /**
    *   @brief          获得可接力制导的飞机ID
    *   @param[in]      selfPlane           导弹载机的PlaneState_S指针
    *   @param[in]      missileID           需要接力制导的导弹ID
    *   @retval         int                 可接力制导友方飞机ID
    */
    SEA_AIR_API int GetRelayGuidancePlaneID(PlaneState_S* selfPlane, int missileID);

    /**
    *   @brief          设置接力制导模式
    *   @param[in]      selfPlane           导弹载机的PlaneState_S指针
    *   @param[in]      missileID           需要接力制导的导弹ID
    *   @retval         friendID            可接力制导友方飞机ID
    */
    SEA_AIR_API void SetRelayGuidanceMode(PlaneState_S* selfPlane, int missileID, int friendID);

    /**
    *   @brief          杀死指定飞机
    *   @param[in]      planeID             飞机ID
    */
    SEA_AIR_API void killPlane(int planeID);

    /**
    *   @brief          开启摇杆
    */
    SEA_AIR_API void StartJoystick();

    /**
    *   @brief          摇杆控制
    *   @param[in]      planes              飞机数据
    *   @retval         int                 按钮编号
    */
    SEA_AIR_API int OutputJoystick(PlaneState_S* planes);

    /**
    *   @brief          发送ShowDist数据（东大专用）
    *   @param[in]      showDist
    */
    SEA_AIR_API void SendShowDist(ShowDist* showDist);

    /**
    *   @brief          激活飞行器悬停
    */
    SEA_AIR_API void SetEnableHover(PlaneState_S* selfPlane);

    /**
    *   @brief          解除飞行器悬停
    */
    SEA_AIR_API void SetDisableHover(PlaneState_S* selfPlane);

    /**
    *   @brief          获得友方雷达捕获目标（收发分治）
    */
    SEA_AIR_API FriendRaderCapture_S* GetFriendsRaderCaptureInfo(const PlaneState_S* selfPlane);

    /**
    *   @brief          预测导弹命中时间
    *   @param[in]      airID       载机ID
    *   @param[in]      targetID    目标ID
    */
    SEA_AIR_API double PredictTheTimeOfMissileImpact(int airID, int targetID);

    /**
    *   @brief          导弹是否能命中舰艇
    */
    SEA_AIR_API bool IsHitShip(const MissileState_S& missileObj);

    /**
    *   @brief          开启舰艇近防炮
    *   @param[in]      self        舰艇
    *   @param[in]      angle       角度
    */
    SEA_AIR_API void OpenCIWS(const PlaneState_S* self, double angle);

    /**
    *   @brief          关闭舰艇近防炮
    *   @param[in]      airID       舰艇ID
    */
    SEA_AIR_API void CloseCIWS(int airID);


    /**
    * @brief          【新增】强制设置飞机位置（用于航母挂载/瞬移）
    */
    SEA_AIR_API void SetPlanePosition(int planeID, double lon, double lat, double alt, double vn, double ve);

}
