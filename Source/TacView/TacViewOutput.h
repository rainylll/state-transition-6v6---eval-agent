// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @file           TacViewOutput.h
*   @brief          TacView 实体及事件更新。
*   @details        TacView 实体及事件更新，详见 ACMI 格式说明。
*   @author         lidaiwei
*   @date           20200808
*   @version        1.0.0.1
*   @par Copyright
*                   GaoYang
*   @par History
*                   1.0.0.1: lidaiwei, 20200808, 首次创建
*/

#ifndef  TacViewOutput_H
#define  TacViewOutput_H

#include <vector>
#include <string>

#include "TacViewFile_T.h"

#if SEA_AIR_ENABLE_TACVIEW_SERVER
#include "TacViewServer_T.h"
#else
namespace TacView
{
	class TacViewServer_T;
}
#endif

//namespace TacView
//{
//#define RADAR_RANGE					75000.0	   //雷达扫描范围，单位 米
//#define RADAR_HBEAMWIDTH			10.0	   //雷达波束宽度，单位 度
//#define RADAR_VBEAMWIDTH			4.0
//
//#define LatitudeToM					111130.0
//}

class TacViewOutput
{
public:
	TacViewOutput();
	~TacViewOutput();

	// --------------------------------------------------------------------------------------------------------------------------------
	/**
	*   @brief          配置仿真环境并启动服务器
	*   @details        战场信息 需要手动填写
	*   @retval         0               正常
	*   @retval         1               错误
	*   @retval         2               错误 已经打开
	*   @retval         3               错误 WSAStartup 失败
	*   @retval         4               错误 Socket 初始化失败
	*   @retval         5               错误 Socket 绑定失败
	*   @retval         6               错误 Socket 监听失败
	*/
	int InitServer();

	// --------------------------------------------------------------------------------------------------------------------------------
	/**
	*   @brief          配置初始实体状态参数
	*   @details        飞机信息及初始参数需要手动填写
	*   @param[in]      time						时间戳
	*   @param[in]      object_id                   实体编号
	*   @param[in]      simulation_id               仿真唯一ID
	*   @param[in]      base_name                   对象名字，例：F-16
	*   @param[in]      base_type 					类型，例：Air+FixedWing
	*   @param[in]      base_pilot 					飞行员
	*   @param[in]      base_color 					颜色，例：Red Orange Green Blue Violet
	*   @param[in]      base_length	                长，单位：米
	*   @param[in]      base_height	                宽，单位：米
	*   @param[in]      base_color	                高，单位：米
	*   @param[in]      lon                         坐标 经度 单位 度
	*   @param[in]      lat                         坐标 纬度 单位 度
	*   @param[in]      alt                         坐标 高度 单位 米
	*   @param[in]      roll                        姿态 横滚角 单位 度
	*   @param[in]      pitch                       姿态 俯仰角 单位 度
	*   @param[in]      yaw                         姿态 偏航角 单位 度
	*   @retval         0               正常
	*   @retval         1               错误
	*/
	int InitOneObject(
		double time,
		int object_id,
		int simulation_id,
		std::string base_name,
		std::string base_type,
		std::string base_pilot,
		std::string base_color,
		double base_length,
		double base_width,
		double base_height,
		double lon,
		double lat,
		double alt,
		double roll,
		double pitch,
		double yaw,
		bool   radar_opened,
		bool   isRadarModeChange,
		double flight_radar_azimuth,
		double flight_radar_elevation,
		double flight_radar_range,
		double flight_radar_hbeamwidth,
		double flight_radar_vbeamwidth,
		double flight_radar_all_hBeamWidth,
		double flight_radar_all_vBeamWidth,
		double flight_radar_all_range,
		double flight_radar_all_azimuth,
		double flight_radar_all_elevation
	);

	// --------------------------------------------------------------------------------------------------------------------------------
	/**
	*   @brief          发送一帧的数据
	*   @details        发送一帧的数据
	*   @param[in]      frame_time            仿真时间戳
	*   @param[in]      object_count          一帧中的实体数
	*   @param[in]      event_count           一帧中的事件数
	*   @retval         0               正常
	*   @retval         1               错误
	*/
	int SendOneFrame(
		double frame_time);

	int SendRadarFrame(
		double frame_time);

	int SendAddCircleFrame(
		double frame_time);

	int SendRemoveCircleFrame(
		double frame_time);

	int SendBoundaryFrame(
		double frame_time);

	int SendDrawCrossGridFrame(
		double frame_time);

	int SendPathPlanFrame(
		double frame_time);

	// --------------------------------------------------------------------------------------------------------------------------------
	/**
	*   @brief          填写一帧中一架飞机的信息
	*   @details        填写一帧中一架飞机的信息
	*   @param[in]      time						时间戳 
	*   @param[in]      object_id                   实体编号
	*   @param[in]      flight_id                   仿真唯一ID
	*   @param[in]      flight_valid                飞机存活  1-存活  2-死亡
	*   @param[in]      lon                         坐标 经度 单位 度
	*   @param[in]      lat                         坐标 纬度 单位 度
	*   @param[in]      alt                         坐标 高度 单位 米
	*   @param[in]      roll                        姿态 横滚角 单位 度
	*   @param[in]      pitch                       姿态 俯仰角 单位 度
	*   @param[in]      yaw                         姿态 偏航角 单位 度
	*   @param[in]      radar_opened                雷达开关机状态
	*   @param[in]      flight_locked_target_id     雷达锁定目标的仿真唯一ID（负数表示没有锁定任何目标）
	*   @param[in]      flight_radar_azimuth        雷达波束方向 方位角 单位 度
	*   @param[in]      flight_radar_elevation      雷达波束方向 俯仰角 单位 度
	*   @param[in]      flight_radar_range			雷达扫描距离，单位 米
	*   @retval         0               正常
	*   @retval         1               错误
	*/
	int OneFrameFlightState(
		double time,
		int object_id,
		int flight_id,
		int flight_valid,
		double lon,
		double lat,
		double alt,
		double roll,
		double pitch,
		double yaw,
		bool radar_opened,
		bool isRadarModeChange,
		int flight_locked_target_id,
		double flight_radar_azimuth,
		double flight_radar_elevation,
		double flight_radar_range,
		double flight_radar_hbeamwidth,
		double flight_radar_vbeamwidth,
		double flight_radar_all_hBeamWidth,
		double flight_radar_all_vBeamWidth,
		double flight_radar_all_range,
		double flight_radar_all_azimuth,
		double flight_radar_all_elevation
	);

	// --------------------------------------------------------------------------------------------------------------------------------
	/**
	*   @brief          填写一帧中一个导弹的信息
	*   @details        填写一帧中一个导弹的信息
	*   @param[in]      object_id                   实体编号
	*   @param[in]      missile_id                  仿真唯一ID
	*   @param[in]      missile_color               导弹颜色 （“Red”，“Blue”，“Orange”）
	*   @param[in]      missile_valid               导弹存活  1-存活  2-死亡
	*   @param[in]      lon                         坐标 经度 单位 度
	*   @param[in]      lat                         坐标 纬度 单位 度
	*   @param[in]      alt                         坐标 高度 单位 米
	*   @param[in]      roll                        姿态 横滚角 单位 度
	*   @param[in]      pitch                       姿态 俯仰角 单位 度
	*   @param[in]      yaw                         姿态 偏航角 单位 度
	*   @param[in]      missile_target_id           末制导锁定目标的仿真唯一ID（负数表示没有锁定任何目标）
	*   @retval         0               正常
	*   @retval         1               错误
	*/
	int OneFrameMissileState(
		int object_id,
		int missile_id,
		std::string missile_color,
		int missile_valid,
		double lon,
		double lat,
		double alt,
		double roll,
		double pitch,
		double yaw,
		int missile_target_id);

	int OneFrameRadarState(
		int object_id,
		int flight_id,
		int flight_live,
		bool radar_opened,
		int flight_locked_target_id,
		double flight_radar_azimuth,
		double flight_radar_elevation,
		double flight_radar_range,
		double flight_radar_horizontal_beamwidth,
		double flight_radar_vertical_beamwidth,
		double flight_radar_all_hBeamWidth,
		double flight_radar_all_vBeamWidth,
		double flight_radar_all_range,
		double flight_radar_all_azimuth,
		double flight_radar_all_elevation
		);

	int OneFrameBoundaryState(
		int num,
		double baseLongitude, double baseLatitude, double baseAltitude,
		double xLen, double yLen, double zLen,
		int color
	);

	int OneFrameVisableState(
		double time,
		int object_id,
		int flight_id,
		bool isVisable
	);

	int OneFrameDrawCrossGrid(const TacView::CrossGrid& crossGrid);

	int OneFrameAddCircle(const TacView::CircleData& circleData);
	int OneFrameRemoveCircle(const TacView::CircleData& circleData);

	int OneFramePathPlanState(const TacView::PathTomonitor_S& pathTomonitor);

	TacView::TacViewServer_T* server;
	TacView::State_T* state;

private:
	std::vector<int> List_missile_id;
};


#endif // TacViewOutput_H

