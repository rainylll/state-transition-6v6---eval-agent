#pragma once
#include <iostream>
#include <vector>
#include "uavDMInterface.h"

const double PI = 3.14159265358;
const double R_to_GR = (180.0 / PI);
const double GR_to_R = (PI / 180.0);
const double EARTH_RADIUS = 6371000.0;
const double EARTH_G = 9.8;
const float KNOT_ONE = 0.5144f;

// 11.10新增——飞行器参数（参考刘老师文档，后期根据需求和测试变化）
const double MAX_Speed = 1.6;                           // 最大速度MA
const double Cruising_Speed = 1.0;                      // 巡航速度
const double Overloading_RangeLow = -2;                 // 过载下限
const double Overloading_RangeHigh = 7.5;               // 过载上限
const double MAX_Height = 18300;                        // 最大高度
const double MIN_Height = 0;                            // 最小高度
const double MAX_AttackDis = 120000;                    // 最大攻击距离
const double Min_AttackDis = 10000;						// 最小攻击距离
const double radarMinDis = 5000;						// 雷达最小探测距离
const double MAX_DecTime = 30;                          // 最大探测时间
const double OpenCloseRadarPeriod = 7;                  // 间歇开关雷达周期

const float Max_Off_Axis_Angle = 30;                    // 最大离轴发射角（非控制量）
const double Optimal_Speed = 408;						// 最佳速度（固定1.2MA）
const double Optimal_Height = 10000;					// 最佳高度（每回合计算一次，躲避完成后计算！）
const double Cruise_Height = 8000;						// 巡航高度
const double Maneuver_Speed = 340;						// 机动速度
const double Crusie_Speed = 340;						// 巡航速度

// 12.11--高度和速度分级
const double Speed_Level1 = 1.4 * 340;
const double Speed_Level2 = 1.2 * 340;
const double Speed_Level3 = 1.0 * 340;
const double Speed_Level4 = 0.8 * 340;
const double Speed_Level5 = 0.6 * 340;
const double Height_Level1 = 11000;
const double Height_Level2 = 10000;
const double Height_Level3 = 8000;
const double Height_Level4 = 6000;
const double Height_Level5 = 4000;

// 11.11新增——模式常量

enum ActionMode{
	Search_Mode = 0,       // 搜索模式
	Medium_Guidance_Mode, // 中制导模式
	Attack_Mode,          // 攻击模式
	Evade_Mode,            // 躲避模式
	Detour_Mode                   // 游击模式
};



struct Point {
	//七个方向的点
	int planeID;
	double x;
	double y;
	double z;
	double roll;
	double pitch;
	double azimuth;
	double v;
	double time;

	float V_cmd;
	float H_cmd;
	float A_cmd;

	Point();
	Point(double x, double y, double z, double azimuth, double v, float V_cmd, float H_cmd, float A_cmd);
	Point(double x, double y, double z);
	Point(double x, double y, double z, double azimuth, double pitch, double v);
	Point(double x, double y, double z, double azimuth, double v);
	Point(int planeID, double x, double y, double z, double azimuth, double pitch, double roll, double v, float V_cmd, float H_cmd, float A_cmd);
	bool operator == (const Point& p)
	{
		return x == p.x && y == p.y;
	}
	bool operator < (const Point& pk) const
	{
		return  x < pk.x;
	}
	//Point(int planeID, double x, double y, double z, double azimuth, double v);
};

struct DecProcess
{
	int Last_Mode;		  //上一时刻处于什么场景
	int mode;			  // 判断当前场景
	std::vector<int> warnNum; //敌机导弹末制导告警数量
	std::vector<int> RadatWarnNum; //敌机雷达告警信息
	bool isLastWarn;      // 上一时刻是否有末制导
	int againevade;       //连续两次躲避标志位   0不需要  1开始躲避  2躲避后续标志
	int curMissleNumber;  // 当前发射导弹序号
	int Enemy_curMissleNumber; //敌机导弹发射统计
	long long Time;     //实际战场时间（单位s）
	long long Temp_Time; //临时计时标志位（在不同模块重复使用)
	double Fire_Dist[MAX_MISSILE_NUM];    //导弹发射距离
	std::vector<double> Last_Enemy_Location;				  // 上时刻敌机经纬信息
	double Desire_Height; //最佳高度(每回合计算一次)
	double Attack_Count;
	int MissleNumber; //导弹发射数
	double Dist; //敌我距离
	double Last_Dist; //历史敌我距离
	double Azimuth; //敌我方位角
	double Azimuth_H; //历史敌我方位角
	float Off_Axis_Angle; //离轴角
	double Missile_Dist[MAX_MISSILE_NUM]; //导弹与敌机的距离
	int attackSitutation;                 // 敌我的攻击态势
	int Situation_Score;
	int Missile_Live_Number; //当前导弹存活数

	bool Is_Maneuver_Right; //是否满足攻击约束
	int tgtWaringTimes;    // 敌方雷达可以扫描到本机的时间记录 
	int missileShootTimeSpace;  //发射一枚导弹的时间间隔 

	int evadeCurWarnNum;         // 连续躲避此时告警数量      
	double modelTime[6];
	double startTimeOfMozhidao;

	DecProcess()
	{
		Off_Axis_Angle = -1;
		Last_Mode = -1;
		mode = -1;
		isLastWarn = false;
		curMissleNumber = 0;
		Enemy_curMissleNumber = 0;
		Time = 0;
		Temp_Time = 0;
		Desire_Height = 10000; //初始10000m
		memset(&Fire_Dist, 0, sizeof(double)*MAX_MISSILE_NUM);
		Last_Enemy_Location = { -1, -1 };
		Attack_Count = 0;
		MissleNumber = 0;
		Dist = 0;
		Azimuth = 0;
		Azimuth_H = 0;
		memset(&Missile_Dist, 0, sizeof(double)*MAX_MISSILE_NUM);
		attackSitutation = 0;
		Is_Maneuver_Right = false;
		tgtWaringTimes = 0;
		Situation_Score = -1;
		Missile_Live_Number = 0;
		missileShootTimeSpace = 0;
		againevade = 0;
		evadeCurWarnNum = 0;
		startTimeOfMozhidao = -1;
		for (int i = 0; i < 6; i++)
		{
			modelTime[i] = -1;
		}
	};
};



double DisOfTwoPoint(double slog, double slat, double elog, double elat);                                           // 计算大地两点之间的距离 
float Cal_Heading_T(double Long, double Lat, double long_T, double lat_T);                                          // 期望航向计算  坐标系为 正北0度，顺时针为正， 0到360
std::pair<double, double> CompPosition(float delta_de_inm, float delta_dn_inm, double longitude, double latitude);  // 方位角用经纬度计算,方位角单位角度
std::pair<double, double> generateLon_Lat(double s_lon, double s_lat, double angle, double distance);               // 东向位移 北向位移    自身经纬度引用

Point toXYZ(double lon, double lat, double h);                                                                      // 经纬度转成XYZ         
double angleStandardization(double angle);																			// 角度标准化
double angletorad(double angle);																					//角度转换函数,角度转化成0-2pi
double radtoangle(double rad);																						//角度转换函数，弧度变成角度，在 0 到 360 之间
double Pinth_Angle_Range_conversion(double angle);																	//确保将角度转换为[-pi/2, pi/2)
double Angle_Range_conversion(double angle);																		//确保将角度转换为[-pi,pi)
double Get_Desire_Speed(const double Velocity); //获取阶段速度
double Get_Desire_Height(const double Height); //获取阶段高度
bool isAngleEnough(double eDelta,double angle1,double angle2);     // 输入角度为角度
double isAngleEnough_off_axis(double angle1, double angle2);                                                   // 输入角度为角度

double distance(double mine_x, double mine_y, double mine_z, double enemy_x, double enemy_y, double enemy_z);       //计算敌我距离
double distance(double mine_x, double mine_y, double enemy_x, double enemy_y);                                      //计算敌我距离