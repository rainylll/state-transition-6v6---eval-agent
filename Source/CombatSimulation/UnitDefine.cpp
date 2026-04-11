// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @file           UnitDefine.cpp
*   @brief          部队功能实现。
*   @details        部队功能实现。
*   @author         lidaiwei
*   @date           20200908
*   @version        1.0.0.1
*   @par Copyright
*                   GaoYang
*   @par History
*                   1.0.0.1: lidaiwei, 20200908, 首次创建
*
*/

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @name           头文件。
*   @{
*/
#include "UnitDefine.h"

using namespace Eigen;
using namespace CombatSimulation;
/** @}  */

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          设置单位坐标
*   @details        设置单位坐标
*   @param[in]      in_coordinate_longitude             坐标，经度，单位：度
*   @param[in]      in_coordinate_latitude              坐标，纬度，单位：度
*   @param[in]      in_coordinate_altitude              坐标，高度，单位：米
*   @param[in]      in_coordinate_roll                  坐标，滚转角，单位：度
*   @param[in]      in_coordinate_pitch                 坐标，俯仰角，单位：度
*   @param[in]      in_coordinate_yaw					坐标，偏航角，单位：度
*   @retval         0                    正常
*/
int Unit_Object_C::SetCoordinate(
	double							in_coordinate_longitude,
	double							in_coordinate_latitude,
	double							in_coordinate_altitude,
	double							in_coordinate_roll,
	double							in_coordinate_pitch,
	double							in_coordinate_yaw)
{
	coordinate_longitude = in_coordinate_longitude;
	coordinate_latitude = in_coordinate_latitude;
	coordinate_altitude = in_coordinate_altitude;
	coordinate_roll = in_coordinate_roll;
	coordinate_pitch = in_coordinate_pitch;
	coordinate_yaw = in_coordinate_yaw;

	return CS_OK;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          设置单位速度
*   @details        设置单位速度
*   @param[in]      in_velocity_north             速度，北向，单位：米/秒
*   @param[in]      in_velocity_east              速度，东向，单位：米/秒
*   @param[in]      in_velocity_downward          速度，地面方向，单位：米/秒
*   @retval         0                    正常
*/
int Unit_Object_C::SetVelocity(
	double							in_velocity_north,
	double							in_velocity_east,
	double							in_velocity_downward)
{
	velocity_north = in_velocity_north;
	velocity_east = in_velocity_east;
	velocity_downward = in_velocity_downward;

	return CS_OK;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          初始化一个飞机实体
*   @details        初始化一个飞机实体
*   @param[in]      in_simulation_id        仿真id，唯一标识
*   @param[in]      in_base_name            对象名字，例：F-16
*   @param[in]      in_base_team            所属队伍 1-红方 2-蓝方
*   @param[in]      in_lon                  坐标，经度，单位：度
*   @param[in]      in_lat                  坐标，纬度，单位：度
*   @param[in]      in_alt                  坐标，高度，单位：米
*   @param[in]      in_roll                 坐标，滚转角，单位：度
*   @param[in]      in_pitch                坐标，俯仰角，单位：度
*   @param[in]      in_yaw					坐标，偏航角，单位：度
*   @param[in]      in_velocity_north       速度，北向，单位：米/秒
*   @param[in]      in_velocity_east		速度，东向，单位：米/秒
*   @param[in]      in_velocity_downward	速度，地面方向，单位：米/秒
*   @retval         0                    正常
*/
int Aircraft_Object_C::Init(
	int in_simulation_id,
	EquipmentType_E equipmentType,
	int    in_base_team,
	double in_lon,
	double in_lat,
	double in_alt,
	double in_roll,
	double in_pitch,
	double in_yaw,
	double in_velocity_north,
	double in_velocity_east,
	double in_velocity_downward)
{
	Sim_id = in_simulation_id;
	base_live = CS_LIVE;

	//自定义模型
	//if ( in_simulation_id >= 25 && in_simulation_id <= 27 )
	//{
	//	strcpy_s(base_type, max_str, "Ground+AntiAircraft");
	//}
	//else if ( in_simulation_id == 28 )
	//{
	//	strcpy_s(base_type, max_str, "Sea+Watercraft+Warship");
	//}
	//else
	//{
	//	strcpy_s(base_type, max_str, "Air+FixedWing");
	//}

	
	if (equipmentType == ARMORED_CAR)
	{
		strcpy_s(base_name, max_str, "HQ-7 LN");
		strcpy_s(base_type, max_str, "Ground+AntiAircraft");
	}
	else if (equipmentType == SUICIDE_DRONE)
	{
		strcpy_s(base_name, max_str, "MQ-9 Reaper");
		strcpy_s(base_type, max_str, "Air+FixedWing");
	}
	else if (equipmentType == FRIGATE1)
	{
		strcpy_s(base_name, max_str, "Luyang III");
		strcpy_s(base_type, max_str, "Sea+Watercraft+Warship,EngagementRange=0");
	}
	else if (equipmentType == AWACS_B52)
	{
		strcpy_s(base_name, max_str, "B-52 Stratofortress");
		strcpy_s(base_type, max_str, "Air+FixedWing,EngagementRange=0");
	}
	else if (equipmentType == AWACS_E3)
	{
		strcpy_s(base_name, max_str, "E-3 Sentry");
		strcpy_s(base_type, max_str, "Air+FixedWing,EngagementRange=0");
	}
	else if (equipmentType == AIRCRAFTCARRIER_SHANDONG)
	{
		strcpy_s(base_name, max_str, "Liaoning");
		strcpy_s(base_type, max_str, "Sea+Watercraft+Warship,EngagementRange=0");
	}
	else if ( equipmentType == FIGHTER_J15 )
	{
		strcpy_s(base_name, max_str, "Su-33 Flanker-D");
		strcpy_s(base_type, max_str, "Air+FixedWing");
	}
	else
	{
		strcpy_s(base_name, max_str, "F35");
		strcpy_s(base_type, max_str, "Air+FixedWing");
	}

	base_team = in_base_team;

	coordinate_longitude = in_lon;
	coordinate_latitude = in_lat;
	coordinate_altitude = in_alt;
	coordinate_roll = in_roll;
	coordinate_pitch = in_pitch;
	coordinate_yaw = in_yaw;

	radar_state = 1;
	_radar_azimuth = 0;
	_radar_elevation = 0;

	double xn, yn, zn;
	earth_to_navigation(&xn, &yn, &zn, coordinate_longitude, coordinate_latitude, coordinate_altitude,
		p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);
	Vector4d qbn;
	euler_to_quaternion_bn(&qbn, coordinate_roll, coordinate_pitch, coordinate_yaw);

	craft_state << xn, yn, zn, 0.0,
		in_velocity_north, in_velocity_east, in_velocity_downward, 0.,
		qbn(0), qbn(1), qbn(2), qbn(3),
		0, 0, 0, 0;

	return CS_OK;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          飞机实体单步解算
*   @details        飞机实体单步解算
*   @param[in]      d_time          单步时间间隔 单位：秒
*   @retval         0               正常
*   @retval         -1              飞机已死亡
*/
int Aircraft_Object_C::Run(double d_time)
{
	if ( base_live != 1 ) {
		return CS_NOT_LIVE;
	}

	//飞机状态解算
	Flight(&craft_state, craft_state, d_time, craft_handle);

	//坐标转换

	navigation_to_earth(&coordinate_longitude, &coordinate_latitude, &coordinate_altitude,
		craft_state(0, 0), craft_state(0, 1), craft_state(0, 2),
		p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);

	quaternion_bn_to_euler(&coordinate_roll, &coordinate_pitch, &coordinate_yaw, craft_state.row(2));

	velocity_north = craft_state(1, 0);
	velocity_east = craft_state(1, 1);
	velocity_downward = craft_state(1, 2);

	return CS_OK;
}



// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          初始化一个导弹实体
*   @details        初始化一个导弹实体
*   @param[in]      in_simulation_id        仿真id，唯一标识
*   @param[in]      in_base_name            对象名字，例：F-16
*   @param[in]      in_base_team            所属队伍 1-红方 2-蓝方
*   @param[in]      in_lon                  坐标，经度，单位：度
*   @param[in]      in_lat                  坐标，纬度，单位：度
*   @param[in]      in_alt                  坐标，高度，单位：米
*   @param[in]      in_roll                 坐标，滚转角，单位：度
*   @param[in]      in_pitch                坐标，俯仰角，单位：度
*   @param[in]      in_yaw					坐标，偏航角，单位：度
*   @param[in]      in_velocity_north       速度，北向，单位：米/秒
*   @param[in]      in_velocity_east		速度，东向，单位：米/秒
*   @param[in]      in_velocity_downward	速度，地面方向，单位：米/秒
*   @retval         0                    正常
*/
int Missile_Object_C::Init(
	int in_simulation_id,
	std::string in_base_name,
	int    in_base_team,
	double in_lon,
	double in_lat,
	double in_alt,
	double in_roll,
	double in_pitch,
	double in_yaw,
	double in_velocity_north,
	double in_velocity_east,
	double in_velocity_downward)
{
	Sim_id = in_simulation_id;
	base_live = CS_LIVE;

	strcpy_s(base_name, max_str, in_base_name.c_str());

	strcpy_s(base_type, max_str, "Weapon+Missile");
	base_team = in_base_team;

	coordinate_longitude = in_lon;
	coordinate_latitude = in_lat;
	coordinate_altitude = in_alt;
	coordinate_roll = in_roll;
	coordinate_pitch = in_pitch;
	coordinate_yaw = in_yaw;

	//东大团队
	no_lead_time = -0.3;

	radar_state = 0; //末制导初始化关闭

	double xn, yn, zn;
	earth_to_navigation(&xn, &yn, &zn, coordinate_longitude, coordinate_latitude, coordinate_altitude,
		p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);
	Vector4d qbn;
	euler_to_quaternion_bn(&qbn, coordinate_roll, coordinate_pitch, coordinate_yaw);

	missile_state << xn, yn, zn, 0.0,
		in_velocity_north, in_velocity_east, in_velocity_downward, 0.,
		qbn(0), qbn(1), qbn(2), qbn(3),
		0, 0, 0, 0;

	missile_journey = 0;
	_isView = true;

	missile_position_last = missile_state;
	//东大团队
	missile_position_last1 = missile_state;
	missile_position_last2 = missile_position_last1;

	return CS_OK;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          导弹实体单步解算
*   @details        导弹实体单步解算
*   @param[in]      d_time          单步时间间隔 单位：秒
*   @retval         0               正常
*   @retval         -1              导弹已死亡
*/
int Missile_Object_C::Run(
	double d_time)
{
	if ( missile_live != 1 ) {
		return CS_NOT_LIVE;
	}
	Matrix4d target_state;
	if ( lead_state == 1 || self_lead )
	{
		target_state = p_target_air->craft_state;
		last_target_state = target_state;
	}
	else
	{
		target_state = last_target_state;
	}

	if ( lead_state == 0 && !self_lead )
	{
		no_lead_time += 0.1;
	}
	/*if ( self_lead && radar_state == 1 )
	{
		no_self_lead_time += 0.5;
	}*/
	/*cout << "(1)lead_state = " << lead_state << endl;
	cout << "(2)radar_state = " << radar_state << endl;
	cout << "(3)no_lead_time = " << no_lead_time << endl;
	cout << "(4)no_self_lead_time = " << no_self_lead_time << endl;*/
	Vector3d TargetMissile;
	//导弹与目标距离（米）
	distance_target = (target_state.row(0) - missile_state.row(0)).norm();
	if (distance_target < terminal_guidance_distance && radar_state == 0)
	{
		self_lead = true;
		radar_state = 1;	//末制导标志
	}
	//东大补充6.20
	/*if (self_lead != 0) 
	{
		self_lead = radar_state;
	}*/
	//东大补充6.20
	// 
	//导弹运动目标点坐标
	double K_target = 2 * target_state.row(1).norm() / (0.001 * (distance_target));//瞄准目标运动方向的前方
	if ( distance_target <= destroy_range * 15 )
	{
		K_target = 1000 * target_state.row(1).norm() / missile_state.row(1).norm();
	}
	if ( distance_target <= destroy_range * 2 )
	{
		K_target = 200 * target_state.row(1).norm() / missile_state.row(1).norm();
	}

	TargetMissile << target_state(0, 0) + (target_state(1, 0) / target_state.row(1).norm()) * K_target,
		target_state(0, 1) + (target_state(1, 1) / target_state.row(1).norm()) * K_target,
		target_state(0, 2) + (target_state(1, 2) / target_state.row(1).norm()) * K_target;

	//导弹命中判定
	int t_m_s = HitCheck();
	if ( no_self_lead_time >= 2 || no_lead_time >= 2 )
	{
		t_m_s = CS_NOT_LIVE;
	}

	if ( t_m_s < 0 ) {
		missile_live = t_m_s;
		base_live = 0;
		return t_m_s;
	}
	else {
		missile_live = 1;
		base_live = 1;
	}
	double acc = 0;   // 6.24改了
	if ( distance_target < 40000 ) {
		acc = 80;
	}
	else
	{
		acc = 200;
	}
	Flight_find_point(&missile_handle, &missile_errA, &missile_errP, &missile_errR,
		&missile_errAsum, &missile_errPsum, missile_state, acc, d_time, TargetMissile);

	//导弹飞行计算
	missile_Flight(&missile_state, missile_state, d_time, missile_handle);

	//坐标转换
	navigation_to_earth(&coordinate_longitude, &coordinate_latitude, &coordinate_altitude,
		missile_state(0, 0), missile_state(0, 1), missile_state(0, 2),
		p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);

	quaternion_bn_to_euler(&coordinate_roll, &coordinate_pitch, &coordinate_yaw, missile_state.row(2));

	velocity_north = missile_state(1, 0);
	velocity_east = missile_state(1, 1);
	velocity_downward = missile_state(1, 2);

	return CS_OK;
}

int CombatSimulation::Missile_Object_C::Run_Test()
{
	double dt = 0.1;
	predictedTime += dt;
	if ( flyState == Missile_Fly_State_E::BEGINNER )
	{
		if ( missile_live != 1 )
		{
			return CS_NOT_LIVE;
		}

		Vector3d TargetMissile;
		//导弹与目标距离（米）
		distance_target = (temp_target_state.row(0) - missile_state.row(0)).norm();

		//导弹运动目标点坐标
		double K_target = 2 * temp_target_state.row(1).norm() / (0.001 * (distance_target));//瞄准目标运动方向的前方

		TargetMissile << temp_target_state(0, 0) + (temp_target_state(1, 0) / temp_target_state.row(1).norm()) * K_target,
			temp_target_state(0, 1) + (temp_target_state(1, 1) / temp_target_state.row(1).norm()) * K_target,
			temp_target_state(0, 2) + (temp_target_state(1, 2) / temp_target_state.row(1).norm()) * K_target;

		Flight_find_point(&missile_handle, &missile_errA, &missile_errP, &missile_errR,
			&missile_errAsum, &missile_errPsum, missile_state, 20, dt, TargetMissile);

		//导弹飞行计算
		missile_Flight(&missile_state, missile_state, dt, missile_handle);

		//坐标转换
		navigation_to_earth(&coordinate_longitude, &coordinate_latitude, &coordinate_altitude,
			missile_state(0, 0), missile_state(0, 1), missile_state(0, 2),
			p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);

		quaternion_bn_to_euler(&coordinate_roll, &coordinate_pitch, &coordinate_yaw, missile_state.row(2));

		velocity_north = missile_state(1, 0);
		velocity_east = missile_state(1, 1);
		velocity_downward = missile_state(1, 2);

		if ( coordinate_altitude >= fireInitAltitude )
			flyState = Missile_Fly_State_E::MID;
		else
			return CS_OK;
	}

	if ( flyState == Missile_Fly_State_E::MID )
	{
		double sea_h = 10.0; // 掠海高度（相对海面10米）

		temp_target_state = p_target_air->craft_state;
		int res = Run(dt);

		if ( coordinate_altitude < sea_h )
		{
			coordinate_altitude	 = sea_h;

			//double new_lon, new_lat, new_h, actual_sea_h;

			//double sea_level_h = getEGM96SeaLevel(coordinate_longitude, coordinate_latitude);

			//calcSeaSkimmingLLH(coordinate_longitude, coordinate_latitude, coordinate_altitude, sea_h, 
			//	sea_level_h, new_lon, new_lat, new_h, actual_sea_h);

			//std::cout << "当前经纬高：\n";
			//std::cout << "经度：" << coordinate_longitude << "°，纬度：" << coordinate_latitude << "°，高度：" << coordinate_altitude << "米\n";
			//
			//std::cout << "掠海飞行参数：\n";
			//std::cout << "经度：" << new_lon << "°，纬度：" << new_lat << "°\n";
			//std::cout << "大地高（相对于椭球面）：" << new_h << "米\n";
			//std::cout << "实际相对海面高度（约束后）：" << actual_sea_h << "米\n";
			//cout << "========================" << endl;

			//coordinate_longitude = new_lon;
			//coordinate_latitude	 = new_lat;
			//coordinate_altitude	 = new_h;
		}

		return res;
	}

	return CS_OK;
}

int CombatSimulation::Missile_Object_C::Run_SeaSkimming(double d_time)
{
	if ( flyState == Missile_Fly_State_E::BEGINNER )
	{
		if ( missile_live != 1 )
		{
			return CS_NOT_LIVE;
		}

		int bp = 0;
		while ( 1 )
		{
			Vector3d TargetMissile;
			//导弹与目标距离（米）
			distance_target = (temp_target_state.row(0) - missile_state.row(0)).norm();

			if (temp_target_state(0, 2) < 0)
				temp_target_state(0, 2) = 0;

			//导弹运动目标点坐标
			double K_target = 2 * temp_target_state.row(1).norm() / (0.001 * (distance_target));//瞄准目标运动方向的前方

			TargetMissile << temp_target_state(0, 0) + (temp_target_state(1, 0) / temp_target_state.row(1).norm()) * K_target,
				temp_target_state(0, 1) + (temp_target_state(1, 1) / temp_target_state.row(1).norm()) * K_target,
				temp_target_state(0, 2) + (temp_target_state(1, 2) / temp_target_state.row(1).norm()) * K_target;

			Flight_find_point(&missile_handle, &missile_errA, &missile_errP, &missile_errR,
				&missile_errAsum, &missile_errPsum, missile_state, 20, d_time, TargetMissile);

			//导弹飞行计算
			missile_Flight(&missile_state, missile_state, d_time, missile_handle);

			//坐标转换
			navigation_to_earth(&coordinate_longitude, &coordinate_latitude, &coordinate_altitude,
				missile_state(0, 0), missile_state(0, 1), missile_state(0, 2),
				p_battle_header->reference_longitude, p_battle_header->reference_latitude, p_battle_header->reference_altitude);

			quaternion_bn_to_euler(&coordinate_roll, &coordinate_pitch, &coordinate_yaw, missile_state.row(2));

			velocity_north = missile_state(1, 0);
			velocity_east = missile_state(1, 1);
			velocity_downward = missile_state(1, 2);

			if (bp == 100 || coordinate_altitude > 0)
				break;

			bp++;
		}

		if ( coordinate_altitude >= fireInitAltitude )
			flyState = Missile_Fly_State_E::MID;
		else
			return CS_OK;
	}

	if ( flyState == Missile_Fly_State_E::MID )
	{
		double sea_h = 10.0; // 掠海高度（相对海面10米）

		temp_target_state = p_target_air->craft_state;
		int res = Run(d_time);

		if ( coordinate_altitude < sea_h )
		{
			coordinate_altitude	 = sea_h;

			//double new_lon, new_lat, new_h, actual_sea_h;

			//double sea_level_h = getEGM96SeaLevel(coordinate_longitude, coordinate_latitude);

			//calcSeaSkimmingLLH(coordinate_longitude, coordinate_latitude, coordinate_altitude, sea_h, 
			//	sea_level_h, new_lon, new_lat, new_h, actual_sea_h);

			//std::cout << "当前经纬高：\n";
			//std::cout << "经度：" << coordinate_longitude << "°，纬度：" << coordinate_latitude << "°，高度：" << coordinate_altitude << "米\n";
			//
			//std::cout << "掠海飞行参数：\n";
			//std::cout << "经度：" << new_lon << "°，纬度：" << new_lat << "°\n";
			//std::cout << "大地高（相对于椭球面）：" << new_h << "米\n";
			//std::cout << "实际相对海面高度（约束后）：" << actual_sea_h << "米\n";
			//cout << "========================" << endl;

			//coordinate_longitude = new_lon;
			//coordinate_latitude	 = new_lat;
			//coordinate_altitude	 = new_h;
		}
		
		return res;
	}

	return CS_OK;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          导弹命中判定
*   @details        导弹命中判定
*   @retval         0               运行中，未命中
*   @retval         -1              已命中
*   @retval         -2              超出射程，未命中
*/
void Missile_Object_C::getEnergyCostMultiple()
{
	Eigen::Vector2d a;
	Eigen::Vector2d b;
	a << missile_position_last1(0, 0) - missile_position_last2(0, 0), missile_position_last1(0, 1) - missile_position_last2(0, 1);
	b << missile_state(0, 0) - missile_position_last1(0, 0), missile_state(0, 1) - missile_position_last1(0, 1);
	double dot = a(0) * b(0) + a(1) * b(1);
	double magA = std::sqrt(a(0) * a(0) + a(1) * a(1));
	double magB = std::sqrt(b(0) * b(0) + b(1) * b(1));
	double cosTheta = dot / (magA * magB);
	//// 处理可能的浮点数精度问题
	if ( cosTheta > 1.0 ) cosTheta = 1.0;
	if ( cosTheta < -1.0 ) cosTheta = -1.0;
	double da = radtoangle(std::acos(cosTheta));
	angleCostMultiple = abs(da) * angleCostXita + 1;
}

/**
*   @brief          导弹命中判定
*   @details        导弹命中判定
*   @retval         0               运行中，未命中
*	@retval			-1				失去引导
*   @retval         -3              已命中
*   @retval         -2              超出射程，未命中
*/
int Missile_Object_C::HitCheck()
{
	//static Matrix4d missile_position_last = missile_state;
	//static double missile_journey = 0;
	//cout << "distance_target:" << distance_target;
	if ( distance_target <= destroy_range )
	{
		p_target_air->base_live = 0;
		return Target_died;
	}
	if ( no_lead_time >= 10 )
		return CS_NOT_LIVE;

	double d_distance = (missile_state.row(0) - missile_position_last.row(0)).norm();

	//cout << "(66)angleCostMultiple = " << angleCostMultiple << endl;
	missile_journey += d_distance * angleCostMultiple < 0 ? d_distance : d_distance * angleCostMultiple;
	//missile_journey += d_distance * angleCostMultiple;
	angleCostMultiple = 1;//角度惩罚倍数 ，1s用一次
	//cout << "(99)missile_journey:" << missile_journey << endl;
	if ( missile_journey >= max_journey ) {
		return CS_MISS;
	}
	if ( p_target_air->base_live == 0 ) {
		return Target_died;
	}

	missile_position_last = missile_state;

	return CS_LIVE;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          设置参考点坐标
*   @details        设置参考点坐标
*   @param[in]      in_reference_longitude             参考点经度，单位：deg
*   @param[in]      in_reference_latitude              参考点纬度，单位：deg
*   @param[in]      in_reference_altitude              参考点高度，单位：米
*   @retval         0                    正常
*/
int Battlefield_C::InitCoordinate(
	double							in_reference_longitude,
	double							in_reference_latitude,
	double							in_reference_altitude)
{
	battle_header.reference_longitude = in_reference_longitude;
	battle_header.reference_latitude = in_reference_latitude;
	battle_header.reference_altitude = in_reference_altitude;

	return CS_OK;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          飞机发射导弹
*   @details        飞机发射导弹
*   @param[in]      attack_air             发射机
*   @param[in]      target_air             目标机
*   @retval         0                    正常
*/
int Battlefield_C::MissileFire(
	Aircraft_Object_C& attack_air,
	Aircraft_Object_C& target_air)
{
	missile_count++;

	/*double test_distance_target1 = (target_air.craft_state.row(0) - attack_air.craft_state.row(0)).norm();
	double test_distance_target2 = sqrt(pow(fabs(target_air.coordinate_altitude - attack_air.coordinate_altitude), 2)
									+ pow(fabs(target_air.coordinate_latitude - attack_air.coordinate_latitude) * 111000, 2)
									+ pow(fabs(target_air.coordinate_longitude - attack_air.coordinate_longitude) * 102000, 2));*/
	double test_distance_target3 = GetDistance(target_air.coordinate_longitude, target_air.coordinate_latitude, attack_air.coordinate_longitude, attack_air.coordinate_latitude);

	//cout << test_distance_target1 << "|" << test_distance_target2 << "|" << test_distance_target3 << endl;

	if (test_distance_target3 > missile_list[missile_count - 1].max_journey)
	{
		//cout << "发射失败！距离目标太远，当前目标距离为：" << test_distance_target3 << " 米。" << endl;
		return CS_MISS;
	}

	missile_list[missile_count - 1].Init(3000 + missile_count, "AIM-120", attack_air.base_team,
		attack_air.coordinate_longitude, attack_air.coordinate_latitude, attack_air.coordinate_altitude,
		attack_air.coordinate_roll, attack_air.coordinate_pitch, attack_air.coordinate_yaw,
		attack_air.velocity_north, attack_air.velocity_east, attack_air.velocity_downward);

	missile_list[missile_count - 1].father_id = attack_air.Sim_id;
	missile_list[missile_count - 1].p_target_air = &target_air;
	missile_list[missile_count - 1].missile_live = CS_LIVE;

	return CS_OK;
}

int CombatSimulation::Battlefield_C::MissileFire_SeaSkimming(Aircraft_Object_C& attack_air, Aircraft_Object_C& target_air)
{
	missile_count++;

	double test_distance_target3 = GetDistance(target_air.coordinate_longitude, target_air.coordinate_latitude, attack_air.coordinate_longitude, attack_air.coordinate_latitude);

	if (test_distance_target3 > missile_list[missile_count - 1].max_journey)
	{
		return CS_MISS;
	}

	missile_list[missile_count - 1].Init(3000 + missile_count, "AIM-120", attack_air.base_team,
		attack_air.coordinate_longitude, attack_air.coordinate_latitude, attack_air.coordinate_altitude,
		attack_air.coordinate_roll, 90, attack_air.coordinate_yaw,
		attack_air.velocity_north, attack_air.velocity_east, attack_air.velocity_downward);

	missile_list[missile_count - 1].father_id = attack_air.Sim_id;
	missile_list[missile_count - 1].p_target_air = &target_air;
	missile_list[missile_count - 1].missile_live = CS_LIVE;

	missile_list[missile_count - 1].fireInitAltitude = attack_air.coordinate_altitude + 40;
	double xn, yn, zn;
	earth_to_navigation(&xn, &yn, &zn, attack_air.coordinate_longitude, attack_air.coordinate_latitude, missile_list[missile_count - 1].fireInitAltitude,
		missile_list[missile_count - 1].p_battle_header->reference_longitude, 
		missile_list[missile_count - 1].p_battle_header->reference_latitude, 
		missile_list[missile_count - 1].p_battle_header->reference_altitude);
	Vector4d qbn;
	euler_to_quaternion_bn(&qbn, 0, 90, target_air.coordinate_yaw);

	missile_list[missile_count - 1].temp_target_state << xn, yn, zn, 0.0,
		0, 0, 0, 0.,
		qbn(0), qbn(1), qbn(2), qbn(3),
		0, 0, 0, 0;

	missile_list[missile_count - 1].flyState = Missile_Fly_State_E::BEGINNER;

	return 0;
}

int CombatSimulation::Battlefield_C::MissileFire_Test(Aircraft_Object_C& attack_air, Aircraft_Object_C& target_air)
{
	unsigned int missile_index = max_object - 1;

	if ( missile_list[missile_index].predictedTime > 0 )
	{
		return CS_MISS;
	}

	//Aircraft_Object_C* tempAirObj = new Aircraft_Object_C;
	//memcpy(tempAirObj, &target_air, sizeof(Aircraft_Object_C));
	//double TAS = sqrt(pow(tempAirObj->velocity_east, 2) +
	//	pow(tempAirObj->velocity_north, 2));

	Aircraft_Object_C* tempAirObj = &target_air;
	missile_list[missile_index].p_target_air = tempAirObj;
	missile_list[missile_index].predictedTime = .0;
	missile_list[missile_index].lead_state = 1;

	double test_distance_target3 = GetDistance(
		missile_list[missile_index].p_target_air->coordinate_longitude,
		missile_list[missile_index].p_target_air->coordinate_latitude,
		attack_air.coordinate_longitude,
		attack_air.coordinate_latitude);

	if (test_distance_target3 > missile_list[missile_index].max_journey)
	{
		return CS_MISS;
	}

	missile_list[missile_index].Init(3000 + missile_index, "AIM-120", attack_air.base_team,
		attack_air.coordinate_longitude, attack_air.coordinate_latitude, attack_air.coordinate_altitude,
		attack_air.coordinate_roll, 90, attack_air.coordinate_yaw,
		attack_air.velocity_north, attack_air.velocity_east, attack_air.velocity_downward);

	missile_list[missile_index].father_id = attack_air.Sim_id;
	missile_list[missile_index].missile_live = CS_LIVE;

	missile_list[missile_index].fireInitAltitude = attack_air.coordinate_altitude + 40;
	double xn, yn, zn;
	earth_to_navigation(&xn, &yn, &zn, attack_air.coordinate_longitude, attack_air.coordinate_latitude, missile_list[missile_index].fireInitAltitude,
		missile_list[missile_index].p_battle_header->reference_longitude, 
		missile_list[missile_index].p_battle_header->reference_latitude, 
		missile_list[missile_index].p_battle_header->reference_altitude);
	Vector4d qbn;
	euler_to_quaternion_bn(&qbn, 0, 90, missile_list[missile_index].p_target_air->coordinate_yaw);

	missile_list[missile_index].temp_target_state << xn, yn, zn, 0.0,
		0, 0, 0, 0.,
		qbn(0), qbn(1), qbn(2), qbn(3),
		0, 0, 0, 0;

	missile_list[missile_index].flyState = Missile_Fly_State_E::BEGINNER;

	return CS_OK;
}
