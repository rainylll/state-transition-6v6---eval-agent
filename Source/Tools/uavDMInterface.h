#pragma once
// uavDMInterface.h 为接口文件

#include "eigen337\Eigen\Dense"
/**********************************************************
*  根据需求更改下列常量
****************************************************************/
const int MAX_MISSILE_NUM = 6;                           // 无人机装载导弹量
const int MAX_TARGET_NUMBER = 12;                           // 探测到敌方最大目标数 
const int MAX_WARNING_NUM = 7;                           // 告警最大目标数量 1 + 6 
const double radarMaxDis = 120000;                       // 雷达传感器最大探测距离
const double radarMaxPhiA = 60;							 // 雷达传感器最大探测方位角度 最大范围 [-60, 60]  


/**********************************************************Inint****************************************************************/

// 无人机决策初始化接口
struct DecInit {
	int planeType;                                      // 红方有人机 0 ， 蓝方有人机 1 ，...
	int planeNum;										// 我方飞机编号
	int misNum;											// 我方初始载弹量
	int planeCnt;                                       // 我方飞机数量 ， 1V1暂时用不上
};

/**********************************************************Input****************************************************************/

// 无人机状态信息输入接口
struct UAVDecInput {
	int planeNum;                                       // 我方飞机编号
	bool mIsCrash;      							    // 飞行器是否坠毁 , true  毁坏, false 健康  
	float mYawRad;                                     // 航迹方位角（偏航角）,取值范围[-pi, pi]
    float mRollRad;                                    // 航迹滚转角（滚转角）,取值范围[-pi, pi]
	float mPitchRad;                                   // 航迹倾斜角（俯仰角）,取值范围[-pi/2, pi/2]
	float mVE;                                         // 地面坐标系下，飞机的东向速度
	float mVN;                                         // 地面坐标系下，飞机的北向速度
	float mVS;                                         // 地面坐标系下，飞机的天向速度
	float mTAS;                                        // 真空速
	double mLongitude;                                  // 飞机的经度，范围[-180, 180]
	double mLatitude;								    // 飞机的纬度，范围[-90, 90]
	double mAltitude;								    // 飞机海拔
	//unsigned long long timeStp;                                  // 时间  11.19
};

// 目标状态信息输入接口,从数据融合获取目标数据                
struct EnemyDecInput {
	int cnt;                                            // 探测到敌方飞机数量
	int planeNum[MAX_TARGET_NUMBER];					    // 敌方飞机的编号
	float tVE[MAX_TARGET_NUMBER];						    // 地面坐标系下，敌方飞行器的东向速度
	float tVN[MAX_TARGET_NUMBER];						    // 地面坐标系下，敌方飞行器的北向速度
	float tVS[MAX_TARGET_NUMBER];						    // 地面坐标系下，敌方飞行器的天向速度
	float tYawRad[MAX_TARGET_NUMBER];					    // 航迹方位角（偏航角），取值范围[-pi, pi]
	float tPitchRad[MAX_TARGET_NUMBER];				    // 航迹倾斜角（俯仰角），取值范围[-pi/2, pi/2]
	double tLongitude[MAX_TARGET_NUMBER];				    // 敌方飞机的经度，范围[-180, 180]
	double tLatitude[MAX_TARGET_NUMBER];				    // 敌方飞机的纬度，范围[-90, 90]
	double tAltitutde[MAX_TARGET_NUMBER];				    // 敌方飞机海拔
};

// 火控信息输入接口
struct FireCtrlDecInput {
	float D_PI[MAX_MISSILE_NUM];                                      // 概率截获距离,正实数：120000m    
	float D_MKmax[MAX_MISSILE_NUM];									// 不可逃逸区最大攻击距离,正实数：30000m
	float D_MKmin[MAX_MISSILE_NUM];									// 不可逃逸区最小攻击距离,正实数：20000m
	float Phi_MKmax[MAX_MISSILE_NUM];									// 不可逃逸区圆锥角,范围 [-pi, pi]：2.0944（120°）
	float Phi_Mmax[MAX_MISSILE_NUM];									// 最大离轴发射角,范围 [-pi, pi]：0.610865（35°）
};

struct MissileDecInput {

	bool misIsOpenEye[MAX_MISSILE_NUM];                 // 导弹是否开启末制导， true 开启，false 未开启
	bool misIsLive[MAX_MISSILE_NUM];                    // 导弹是否存活 
	//double misDistance[MAX_MISSILE_NUM];                 // 导弹弹目距
	//double tVE[MAX_MISSILE_NUM];						    // 地面坐标系下，导弹的东向速度
	//double tVN[MAX_MISSILE_NUM];						    // 地面坐标系下，导弹的北向速度
	//double tVS[MAX_MISSILE_NUM];						    // 地面坐标系下，导弹的天向速度
	//double tLongitude[MAX_MISSILE_NUM];				    // 导弹的经度，范围[-180, 180]
	//double tLatitude[MAX_MISSILE_NUM];				    // 导弹的纬度，范围[-90, 90]
	//double tAltitutde[MAX_MISSILE_NUM];				    // 导弹海拔

	//double TarMissleHeading[MAX_MISSILE_NUM];            //敌方导弹和我方位角
	/*double EnemyMissleLon[MAX_MISSILE_NUM];
	double EnemyMissleLat[MAX_MISSILE_NUM];
	double EnemyMissleAlt[MAX_MISSILE_NUM];
	double EnemyMissleTAS[MAX_MISSILE_NUM];
*/



};

// 雷达信息输入接口
struct RadarDecInput {

};

// 电子战信息输入接口（这个和目标状态信息有什么区别？就是目标源类型为0的时候，我先默认只读取导弹末制导告警信息,就是先默认目标源类型为1）
struct ElectronicDecInput {
	int warnCnt;                                        // 警告目标数量
	int type[MAX_WARNING_NUM];                          // 目标源类型，0 敌方飞机 ，1 导弹
	double hTargetAngle[MAX_WARNING_NUM];				// 目标方位角信息   0-360
};

// 无人机决策输入接口
struct DecInput {
	UAVDecInput uavInput; //正常
	EnemyDecInput targetInput; //
	FireCtrlDecInput fireInput; // 均为常量，正常
	MissileDecInput missileInput;
	ElectronicDecInput electronicInput;
	Eigen::Matrix4d					craft_state;							//!< 飞机状态
	double d_time;
	//Eigen::Vector4d					craft_handle;							//!< 飞机控制参数
};

/******************************************************Output********************************************************************/

// 无人机输出接口
struct UAVDecOutput {
	short cmdIndex;                                   // 动作序号 从1开始累加，机动类型变化之后要累加
	unsigned short cmdID;                             // 动作编号
	short iTurnDirection;                             // 转弯方向 1 右转，-1左转 0默认就近转弯
	float  fCmdNyC;                                   // 法向过载  -4~8g
	double eHeadingPath_rad;                          // 期望航迹方位角（偏航角）， 输出范围[-pi, pi]
	double ePitchPath_rad;                            // 期望航迹倾斜角（俯仰角）， 输出范围[-pi/2, pi/2]
	double eRollPath_rad;                             // 期望航迹滚转角（滚转角）， 输出范围[-pi, pi]
	double eSpeed;                                    // 应飞速度（标量） m/s，最大真空速 1.6Ma
	double eHeight;                                   // 应飞高度  
	double eCmdTrust;                                 // 推力大小 
};

// 导弹输出接口
struct MissileDecOutput {
	bool isLaunch[MAX_MISSILE_NUM];                   // 我方飞行器的第i枚导弹是否发射,  true 发射，false 未发射（能不能取到导弹的是否存活？我先默认能取到，后续再改变量）
	int lockOn[MAX_MISSILE_NUM];                      // 我方飞行器的第i枚导弹锁定的目标id 
};


// 雷达输出接口
struct RadarDecOutput {
	bool isOpen;									 // 是否开启雷达， true 开启，false 关闭
};

// 无人机决策输出接口
struct DecOutput {
	UAVDecOutput uavCtrl;                                            //这个没有用
	MissileDecOutput missileCtrl;
	RadarDecOutput radarCtrl;
	Eigen::Vector4d			craft_handle;							//!< 飞机控制参数
};


//躲避全局变量
struct evadeVariables {
	bool IsFirst = true; //主函数用
	int nStep = -1;
	double Estimate_Dist[6] = { 0, 0, 0, 0, 0, 0 };
	int Enemy_Missile_number = 0;
	double desire_Speed = 0; //基于态势计算
	double desire_Height = 0; //基于态势计算
	double desire_angle = 0; //期望航向角
	bool Done = false;
	int Done1 = 0;  //我的
	bool Done2 = false;
	int LR = -1;
	int type = -1;
	double Dist = -1;
	int Missile_Warning_Number = 0;
	bool Escaping = false;
	int Maneuver = -1; //躲避机动
	bool finish_flag = false;
	double t1 = 0;//我的计时
};

struct searchVariables {
	double Desire_Angle = 0;
	int done1 = 0;
	int done2 = 0;
	bool finish_flag = false;
	int  searchState = -1; // 0直飞  1转弯  2转圈
};
struct midVariables {

	double Desire_Angle = 0;
	int done1 = 0;
	bool finish_flag = false;
};

struct detourVariables {
	double Desire_Angle = 0;
	int done1 = 0;
	bool finish_flag = false;
};

struct attackVariables {
	double Desire_Angle = 0;
	int done1 = 0;
	bool manneuver = false;
	bool finish_flag = false;
};


