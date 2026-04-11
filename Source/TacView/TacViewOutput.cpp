#include "TacViewOutput.h"
#include <ctime>
#include "../FlyTac/aircraft.h"

using namespace TacView;
using namespace std;

TacViewOutput::TacViewOutput() 
{
	server = nullptr;
#if SEA_AIR_ENABLE_TACVIEW_SERVER
	server = new TacViewServer_T();
#endif
	state = new State_T;

	memset(state->object, 0, sizeof(Object_T) * max_object);
	memset(state->event, 0, sizeof(Event_T) * max_object);
		
	state->object_count = 0;
	state->event_count = 0;
}

TacViewOutput::~TacViewOutput() 
{
#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if ( server != nullptr )
		delete server;
#endif
	if ( state != nullptr )
		delete state;
}

int TacViewOutput::InitServer(){
	//战场信息 
	Header_T* header;

    
	// 头数据-仿真环境设置
	header = new Header_T;
	memset(header, 0, sizeof((*header)));

    std::time_t now = std::time(nullptr);
    std::tm stUTC{};
    sea_air_gmtime(&now, &stUTC);

    strcpy_s((*header).data_source, max_str, "CASIA SIM Service");
	strcpy_s((*header).data_recorder, max_str, "TacViewHelper");
    sprintf_s((*header).reference_time, max_str, "%d-%02d-%02dT%02d:%02d:%02dZ", stUTC.tm_year + 1900, stUTC.tm_mon + 1, stUTC.tm_mday, stUTC.tm_hour, stUTC.tm_min, stUTC.tm_sec);
    sprintf_s((*header).recording_time, max_str, "%d-%02d-%02dT%02d:%02d:%02dZ", stUTC.tm_year + 1900, stUTC.tm_mon + 1, stUTC.tm_mday, stUTC.tm_hour, stUTC.tm_min, stUTC.tm_sec);
	strcpy_s((*header).author, max_str, "Author");
	strcpy_s((*header).title, max_str, "Aircraft Test");
	strcpy_s((*header).category, max_str, "Intercept Mission");
	strcpy_s((*header).briefing, max_str, "Destroy all");
	strcpy_s((*header).debriefing, max_str, "Fire");
	strcpy_s((*header).comments, max_str, "Go Go Go");
	(*header).reference_longitude = 126.0;
	(*header).reference_latitude = 30.0;
	int fp = 0;
#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		fp = (*server).Open(42674, (*header));
#endif

	if ( header != nullptr )
		delete header;

    return fp;
}

int TacViewOutput::InitOneObject(
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
	double flight_radar_all_elevation)
{
    // 第一帧-初始条件设置
	(*state).time = time;
	(*state).object[object_id].isVisable = true;
	(*state).object[object_id].id = simulation_id;
	(*state).object[object_id].base_valid = 1;
	strcpy_s((*state).object[object_id].base_name, max_str, base_name.c_str());
	strcpy_s((*state).object[object_id].base_type, max_str, base_type.c_str());
	strcpy_s((*state).object[object_id].base_pilot, max_str, base_pilot.c_str());
	strcpy_s((*state).object[object_id].base_country, max_str, "CN");
	strcpy_s((*state).object[object_id].base_coalition, max_str, "Allies");
	strcpy_s((*state).object[object_id].base_color, max_str, base_color.c_str());
	strcpy_s((*state).object[object_id].base_label, max_str, "mod 1.0");
	(*state).object[object_id].base_length = base_length;
	(*state).object[object_id].base_width = base_width;
	(*state).object[object_id].base_height = base_height;

	(*state).object[object_id].coordinate_valid = 1;
	(*state).object[object_id].coordinate_type = 2;
	(*state).object[object_id].coordinate_longitude = lon;
	(*state).object[object_id].coordinate_latitude = lat;
	(*state).object[object_id].coordinate_altitude = alt;
	(*state).object[object_id].coordinate_roll = roll;
	(*state).object[object_id].coordinate_pitch = pitch;
	(*state).object[object_id].coordinate_yaw = yaw;

	(*state).object[object_id].radar_horizontal_beamwidth	= flight_radar_hbeamwidth;
	(*state).object[object_id].radar_vertical_beamwidth		= flight_radar_vbeamwidth;
	(*state).object[object_id].radar_all_hBeamWidth			= flight_radar_all_hBeamWidth;
	(*state).object[object_id].radar_all_vBeamWidth			= flight_radar_all_vBeamWidth;

	(*state).object[object_id].radar_range					= flight_radar_range;
	(*state).object[object_id].radar_all_range				= flight_radar_all_range;

	(*state).object[object_id].radar_azimuth				= flight_radar_azimuth;
	(*state).object[object_id].radar_elevation				= flight_radar_elevation;

	(*state).object[object_id].radar_all_azimuth			= flight_radar_all_azimuth;
	(*state).object[object_id].radar_all_elevation			= flight_radar_all_elevation;

	(*state).object[object_id].radar_valid					= radar_opened;
	(*state).object[object_id].radar_mode					= radar_opened;
	(*state).object[object_id].isRadarModeChange			= true;
	(*state).object[object_id].locked_target_valid			= 0;

	(*state).object[object_id].live							= 1;
	(*state).object[object_id].isInit						= true;

	(*state).object_count++;

	//(*server).Send(*state);
	return 0;
}

int TacViewOutput::SendOneFrame(
	double frame_time) 
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).Send(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendRadarFrame(
	double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendRadar(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendAddCircleFrame(double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendAddCircle(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendRemoveCircleFrame(double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendRemoveCircle(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendBoundaryFrame(double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendBoundary(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendDrawCrossGridFrame(double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendDrawCrossGrid(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::SendPathPlanFrame(double frame_time)
{
	(*state).time = frame_time;

#if SEA_AIR_ENABLE_TACVIEW_SERVER
	if (server != nullptr)
		(*server).SendPathPlan(*state);
#endif

	//memset(state, 0, sizeof((*state)));
	(*state).event_count = 0;
	(*state).object_count = 0;

	return 0;
}

int TacViewOutput::OneFrameFlightState(
	double time,
    int object_id,
    int flight_id,
    int flight_live,
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
)
{
		(*state).time = time;
		(*state).object_count++;

		static bool flight_live_last[max_object] = { 0 };//记录飞机上一时刻存活状态

		(*state).object[object_id].isRadarModeChange = isRadarModeChange;
        (*state).object[object_id].id = flight_id;
		if(flight_live == 1){
			(*state).object[object_id].live = 1; 
		}else{
			(*state).object[object_id].live = 0; 
			if (flight_live_last[object_id]) {//被摧毁的瞬间上报事件
				(*state).event_count++;
				strcpy_s((*state).event[(*state).event_count - 1].type, max_str, "Destroyed");
				(*state).event[(*state).event_count - 1].id_list[(*state).event_count - 1] = (*state).object[object_id].id;
				strcpy_s((*state).event[(*state).event_count - 1].text, max_str, "Boom!!");
			}
		}

		flight_live_last[object_id] = flight_live;
        
		(*state).object[object_id].isRejoin				= false;
		(*state).object[object_id].isVisable			= true;
		(*state).object[object_id].isInit				= false;
		(*state).object[object_id].coordinate_valid		= 1;
		(*state).object[object_id].coordinate_type		= 2;
		(*state).object[object_id].coordinate_longitude = lon;
		(*state).object[object_id].coordinate_latitude  = lat;
		(*state).object[object_id].coordinate_altitude  = alt;
		(*state).object[object_id].coordinate_roll      = roll;
		(*state).object[object_id].coordinate_pitch     = pitch;
		(*state).object[object_id].coordinate_yaw       = yaw;

		if( radar_opened == 1 )
		{
			if( flight_locked_target_id >= 0 )
			{
				(*state).object[object_id].locked_target_valid			= 1;
				(*state).object[object_id].locked_target_mode			= 1;
				(*state).object[object_id].locked_target_id				= flight_locked_target_id;

				(*state).object[object_id].radar_valid					= 1;
				(*state).object[object_id].radar_mode					= 0;
				(*state).object[object_id].radar_range					= 0;
			}
			else
			{
				(*state).object[object_id].radar_valid					= 1;
				(*state).object[object_id].radar_mode					= 1;
				(*state).object[object_id].radar_horizontal_beamwidth	= flight_radar_hbeamwidth;
				(*state).object[object_id].radar_vertical_beamwidth		= flight_radar_vbeamwidth;
				(*state).object[object_id].radar_all_hBeamWidth			= flight_radar_all_hBeamWidth;
				(*state).object[object_id].radar_all_vBeamWidth			= flight_radar_all_vBeamWidth;
				
				(*state).object[object_id].radar_range					= flight_radar_range;
				(*state).object[object_id].radar_all_range				= flight_radar_all_range;

				(*state).object[object_id].radar_azimuth				= flight_radar_azimuth;
				(*state).object[object_id].radar_elevation				= flight_radar_elevation;

				(*state).object[object_id].radar_all_azimuth			= flight_radar_all_azimuth;
				(*state).object[object_id].radar_all_elevation			= flight_radar_all_elevation;

				(*state).object[object_id].locked_target_valid			= 0;
				(*state).object[object_id].locked_target_mode			= 0;
			}
		}
		else
		{
			(*state).object[object_id].radar_valid						= 0;
			(*state).object[object_id].radar_mode						= 0;
			(*state).object[object_id].radar_range						= 0;
			(*state).object[object_id].locked_target_valid				= 0;
			(*state).object[object_id].locked_target_mode				= 0;
		}
		
	return 0;
}

int TacViewOutput::OneFrameMissileState(
	int object_id,
	int missile_id,
	std::string missile_color,
	int missile_live,
	double lon,
	double lat,
	double alt,
	double roll,
	double pitch,
	double yaw,
	int missile_target_id)
{
	(*state).object_count++;

	(*state).object[object_id].id			= missile_id;
	(*state).object[object_id].radar_valid	= 0;
	(*state).object[object_id].isInit		= false;

	if ( std::count(List_missile_id.begin(), List_missile_id.end(), missile_id) == 0 )
	{
		List_missile_id.push_back(missile_id);

		(*state).object[object_id].base_valid = 1;
		strcpy_s((*state).object[object_id].base_name, max_str, "AIM-120");
		strcpy_s((*state).object[object_id].base_type, max_str, "Weapon+Missile");
		//strcpy_s((*state).object[object_id].base_pilot, max_str, std::to_string(missile_id).c_str());
		//strcpy_s((*state).object[object_id].base_country, max_str, "CN");
		//strcpy_s((*state).object[object_id].base_coalition, max_str, "Allies");
		strcpy_s((*state).object[object_id].base_color, max_str, missile_color.c_str());
		//strcpy_s((*state).object[object_id].base_label, max_str, "mod 1.0");
		//(*state).object[object_id].base_length = 5;
		//(*state).object[object_id].base_width = 2;
		//(*state).object[object_id].base_height = 2;

		(*state).object[object_id].isInit = true;
	}
	if ( missile_live == 1 )
	{
		(*state).object[object_id].isVisable = true; 
		(*state).object[object_id].live = 1; 
		(*state).object[object_id].coordinate_valid = 1;
		(*state).object[object_id].coordinate_type = 2;
		(*state).object[object_id].coordinate_longitude = lon;
		(*state).object[object_id].coordinate_latitude  = lat;
		(*state).object[object_id].coordinate_altitude  = alt;
		(*state).object[object_id].coordinate_roll      = roll;
		(*state).object[object_id].coordinate_pitch     = pitch;
		(*state).object[object_id].coordinate_yaw       = yaw;

		if( missile_target_id >= 0 )
		{
			(*state).object[object_id].locked_target_valid = 1;
			(*state).object[object_id].locked_target_mode  = 1;
			(*state).object[object_id].locked_target_id    = missile_target_id;
		}
		else
		{
			(*state).object[object_id].locked_target_valid = 1;
			(*state).object[object_id].locked_target_mode  = 0;			
		}			
	}
	else
	{
		(*state).object[object_id].live = 0; 
		(*state).object[object_id].locked_target_valid = 0;
	}

	return 0;
}

int TacViewOutput::OneFrameRadarState(
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
	)
{
	if ( flight_live == 1 )
	{
		(*state).object[object_id].live = 1;
	}
	else
	{
		(*state).object[object_id].live = 0;
		return 1;
	}

	(*state).object_count++;
	(*state).object[object_id].id = flight_id;
	(*state).object[object_id].coordinate_valid = 1;
	(*state).object[object_id].coordinate_type = 2;

	if ( radar_opened == 1 ) {
		if ( flight_locked_target_id >= 0 ) {
			(*state).object[object_id].locked_target_valid = 1;
			(*state).object[object_id].locked_target_mode = 1;
			(*state).object[object_id].locked_target_id = flight_locked_target_id;

			(*state).object[object_id].radar_valid = 1;
			(*state).object[object_id].radar_mode = 0;
			(*state).object[object_id].radar_range = 0;
		}
		else {
			(*state).object[object_id].radar_valid = 1;
			(*state).object[object_id].radar_mode = 1;
			(*state).object[object_id].radar_range = flight_radar_range;
			(*state).object[object_id].radar_horizontal_beamwidth = flight_radar_horizontal_beamwidth;
			(*state).object[object_id].radar_vertical_beamwidth = flight_radar_vertical_beamwidth;
			(*state).object[object_id].radar_all_hBeamWidth = flight_radar_all_hBeamWidth;
			(*state).object[object_id].radar_all_vBeamWidth = flight_radar_all_vBeamWidth;
			(*state).object[object_id].radar_all_range = flight_radar_all_range;

			(*state).object[object_id].radar_azimuth = flight_radar_azimuth;
			(*state).object[object_id].radar_elevation = flight_radar_elevation;
			(*state).object[object_id].radar_all_azimuth = flight_radar_all_azimuth;
			(*state).object[object_id].radar_all_elevation = flight_radar_all_elevation;

			(*state).object[object_id].locked_target_valid = 1;
			(*state).object[object_id].locked_target_mode = 0;
		}
	}
	else {
		(*state).object[object_id].radar_valid = 1;
		(*state).object[object_id].radar_mode = 0;
		(*state).object[object_id].radar_range = 0;
		(*state).object[object_id].locked_target_valid = 1;
		(*state).object[object_id].locked_target_mode = 0;
	}

	return 0;
}

int TacViewOutput::OneFrameBoundaryState(int num, double baseLongitude, double baseLatitude, double baseAltitude, double xLen, double yLen, double zLen, int color)
{
	(*state).boundaryPoint[0]._longitude = baseLongitude;
	(*state).boundaryPoint[0]._latitude = baseLatitude;
	(*state).boundaryPoint[0]._altitude = baseAltitude;
	
	(*state).boundaryPoint[1]._longitude = LongLatOffset(baseLongitude, baseLatitude, 90.0, xLen).first;
	//(*state).boundaryPoint[1]._longitude = baseLongitude + xLen / LatitudeToM;
	(*state).boundaryPoint[1]._latitude = baseLatitude;
	(*state).boundaryPoint[1]._altitude = baseAltitude;

	(*state).boundaryPoint[2]._longitude = (*state).boundaryPoint[1]._longitude;
	//(*state).boundaryPoint[2]._latitude = baseLatitude - yLen / LatitudeToM;
	(*state).boundaryPoint[2]._latitude = LongLatOffset((*state).boundaryPoint[1]._longitude, (*state).boundaryPoint[1]._latitude, 180.0, yLen).second;
	(*state).boundaryPoint[2]._altitude = baseAltitude;

	(*state).boundaryPoint[3]._longitude = baseLongitude;
	(*state).boundaryPoint[3]._latitude = (*state).boundaryPoint[2]._latitude;
	(*state).boundaryPoint[3]._altitude = baseAltitude;

	(*state).boundaryPoint[4]._longitude = baseLongitude;
	(*state).boundaryPoint[4]._latitude = baseLatitude;
	(*state).boundaryPoint[4]._altitude = zLen;

	(*state).boundaryPoint[5]._longitude = (*state).boundaryPoint[1]._longitude;
	(*state).boundaryPoint[5]._latitude = baseLatitude;
	(*state).boundaryPoint[5]._altitude = zLen;

	(*state).boundaryPoint[6]._longitude = (*state).boundaryPoint[2]._longitude;
	(*state).boundaryPoint[6]._latitude = (*state).boundaryPoint[2]._latitude;
	(*state).boundaryPoint[6]._altitude = zLen;

	(*state).boundaryPoint[7]._longitude = baseLongitude;
	(*state).boundaryPoint[7]._latitude = (*state).boundaryPoint[2]._latitude;
	(*state).boundaryPoint[7]._altitude = zLen;

	for ( int i = 0; i < max_boundary; i++ )
	{
		(*state).boundaryPoint[i]._id = num * 10000 + 00110 + i;
		memset((*state).boundaryPoint[i]._color, 0, color_size);
		switch ( color )
		{
		case Color_E::Red:
			strncpy((*state).boundaryPoint[i]._color, "Red", strlen("Red"));
			break;
		case Color_E::Orange:
			strncpy((*state).boundaryPoint[i]._color, "Orange", strlen("Orange"));
			break;
		case Color_E::Yellow:
			strncpy((*state).boundaryPoint[i]._color, "Yellow", strlen("Yellow"));
			break;
		case Color_E::Green:
			strncpy((*state).boundaryPoint[i]._color, "Green", strlen("Green"));
			break;
		case Color_E::Cyan:
			strncpy((*state).boundaryPoint[i]._color, "Cyan", strlen("Cyan"));
			break;
		case Color_E::Violet:
			strncpy((*state).boundaryPoint[i]._color, "Violet", strlen("Violet"));
			break;
		case Color_E::Blue:
			strncpy((*state).boundaryPoint[i]._color, "Blue", strlen("Blue"));
			break;
		}
	}

	return 0;
}

int TacViewOutput::OneFrameVisableState(double time, int object_id, int flight_id, bool isVisable)
{
	if ( isVisable )
	{
		(*state).object[object_id].live			= 1;
		(*state).object[object_id].isVisable	= true;
		(*state).object[object_id].isRejoin		= true;
	}
	else
	{
		(*state).object[object_id].live		 = 0;
		(*state).object[object_id].isVisable = false;
		(*state).object[object_id].isRejoin	 = false;
	}

	(*state).time = time;
	(*state).object_count++;
	(*state).object[object_id].id = flight_id;

	return 0;
}

int TacViewOutput::OneFrameDrawCrossGrid(const TacView::CrossGrid& crossGrid)
{
	(*state).crossGrid = crossGrid;
	return 0;
}

int TacViewOutput::OneFrameAddCircle(const TacView::CircleData &circleData)
{
	(*state).circleData = circleData;
	return 0;
}

int TacViewOutput::OneFrameRemoveCircle(const TacView::CircleData &circleData)
{
	(*state).circleData = circleData;
	return 0;
}

int TacViewOutput::OneFramePathPlanState(const TacView::PathTomonitor_S& pathTomonitor)
{
	(*state).pathPlan = pathTomonitor;
	return 0;
}
