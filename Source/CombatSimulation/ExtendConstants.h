#pragma once

/* 作废
#define RADAR_RANGE					75000.0			//雷达扫描范围，单位 米
#define RADAR_HBEAMWIDTH			10.0			//雷达波束水平宽度，单位 度
#define RADAR_VBEAMWIDTH			10.0			//雷达波束垂直宽度（角度）
#define RADAR_ALL_HBEAMWIDTH		120.0			//雷达扫描水平宽度（角度）
#define RADAR_ALL_VBEAMWIDTH		0.0				//雷达扫描垂直高度（角度）
*/

//#define SHOW_RADAR_ALL_HBEAMWIDTH				//显示雷达扫描全水平宽度（角度）Tacview
//#define SHOW_RADAR_ALL_VBEAMWIDTH				//显示雷达扫描全垂直高度（角度）Tacview
//#define SHOW_RADAR_ALL_RANGE					//显示雷达全距离 Tacview

//#define SHOW_TACVIEW_RADAR						//Tacview雷达显示

#define LatitudeToM					111130.0

#define MINIMUM_HEIGHT				300				//最小飞行高度

const char DEFAULT_CONFIG	= 0;					//默认配置
const char MAV_RED_1		= 1;					//红方有人机1配置
const char MAV_RED_2		= 2;					//红方有人机2配置
const char UAV_RED_1		= 3;					//红方无人机1配置
const char MAV_BLUE_1		= 4;					//蓝方有人机1配置
const char UAV_BLUE_1		= 5;					//蓝方无人机1配置
const char LIMIT_FRIGATE1	= 6;					//舰艇1配置
const char LIMIT_AWACS_RED1 = 7;					//红方预警机配置
const char LIMIT_J35A		= 8;					//歼35A配置


typedef struct
{
	unsigned int	maxJourney;						//!< 导弹最大射程
	unsigned int	terminalGuidanceDistance;		//!< 末制导距离
	unsigned int	speed;							//!< 速度限制，单位：米/秒
	unsigned int	altitude;						//!< 高度限制，单位：米

	//double			RCS;							//!< RCS值

	double			overload_upper;					//!< 最大过载上限
	double			overload_below;					//!< 最大过载下限

	double			radar_hBeamWidth;				//!< 雷达波束水平宽度，单位 度
	double			radar_vBeamWidth_upper;			//!< 雷达波束垂直宽度（角度）
	double			radar_vBeamWidth_below;			//!< 雷达波束垂直宽度（角度）
	double			radar_view_range;				//!< 雷达视觉范围，单位：米
	double			radar_range;					//!< 雷达，范围，单位：米
	double			radar_all_hbeamwidth;			//!< 雷达扫描水平宽度（角度）
	double			radar_all_vbeamwidth;			//!< 雷达扫描垂直高度（角度）

} Unit_Object_Limit;
