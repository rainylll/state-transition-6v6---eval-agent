// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @file           TacViewFile_T.cpp
*   @brief          TacView 文件生成。
*   @details        TacView 文件生成，详见 ACMI 格式说明。
*   @author         GaoYang
*   @date           20181126
*   @version        1.0.0.1
*   @par Copyright
*                   GaoYang
*   @par History
*                   1.0.0.1: GaoYang, 20181126, 首次创建
*                   1.0.0.2: lidaiwei, 20200808, 修正bug
*/

#include "TacViewFile_T.h"

using namespace TacView;

extern bool SHOW_RADAR_ALL_HBEAMWIDTH;
extern bool SHOW_RADAR_ALL_VBEAMWIDTH;
extern bool SHOW_RADAR_ALL_RANGE;

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @name           静态变量。
*   @{
*/
char                            TacViewFile_T::file_str[net_buffer_size];      
/** @}  */

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          构造函数。
*   @details        构造函数，创建时默认被执行的函数。
*/
TacViewFile_T::TacViewFile_T()
{
	m_file = NULL;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          析构函数。
*   @details        析构函数，销毁时默认被执行的函数。
*/
TacViewFile_T::~TacViewFile_T()
{
	if (m_file != NULL)
	{
		fclose(m_file);
		m_file = NULL;
	}
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          文件打开函数。
*   @details        文件打开函数。
*   @param[in]      file_name       文件名，包含路径。
*   @param[in]      header          战场基本信息，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*   @retval         2               错误 文件打开失败
*/
int TacViewFile_T::Open(
	const char* file_name,
	const Header_T &header)
{
	// 判断是否已经打开
	if (m_file != NULL)
	{
		fclose(m_file);
		m_file = NULL;
	}

	// 生成数据
	BuildString(header, net_buffer_size, file_str);

	// 打开文件，写入数据
	errno_t err_code = fopen_s(&m_file, file_name, "w+t");
	if (err_code == 0)
	{
		fwrite(file_str, lstrlenA(file_str), 1, m_file);
	}
	else
	{
		// 文件打开失败
		return 2;
	}

	return 0;
}

/**
*   @brief          是否已经打开文件
*   @retval         0               关闭
*   @retval         1               打开
*/
int TacView::TacViewFile_T::isOpen()
{
	if ( m_file != NULL )
		return 1;
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          文件关闭函数。
*   @details        文件关闭函数。
*   @retval         0               正常
*   @retval         1               错误
*   @retval         2               错误 文件未打开
*/
int TacViewFile_T::Close()
{
	// 判断文件是否打开
	if (m_file == NULL)
	{
		// 文件未打开
		return 2;
	}

	fclose(m_file);
	m_file = NULL;

	//delete[] file_str;
	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          在文件中添加记录。
*   @details        在文件中添加记录。
*   @param[in]      state           仿真推进内容，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*   @retval         2               错误 文件未打开
*/
int TacViewFile_T::Step(
	const State_T &state)
{
	// 判断文件是否打开
	if (m_file == NULL)
	{
		// 文件未打开
		return 2;
	}

	// 生成数据
	if ( BuildString(state, net_buffer_size, file_str) != -1 )
	{
		// 写入文件
		fwrite(file_str, lstrlenA(file_str), 1, m_file);
	}

	return 0;
}

int TacViewFile_T::StepRadar(
	const State_T& state)
{
	// 判断文件是否打开
	if ( m_file == NULL )
	{
		// 文件未打开
		return 2;
	}

	// 生成数据
	BuildRadarString(state, net_buffer_size, file_str);

	// 写入文件
	fwrite(file_str, lstrlenA(file_str), 1, m_file);

	return 0;
}

int TacView::TacViewFile_T::AddBoundary(const State_T& state)
{
	if ( m_file == NULL )
		return 2;

	BuildBoundaryString(state, net_buffer_size, file_str);
	fwrite(file_str, lstrlenA(file_str), 1, m_file);
	return 0;
}

int TacView::TacViewFile_T::AddPathPlan(const State_T& state)
{
	if ( m_file == NULL )
		return 2;

	BuildPathPlanString(state, net_buffer_size, file_str);
	fwrite(file_str, lstrlenA(file_str), 1, m_file);
	return 0;
}

int TacViewFile_T::addFrame(double dt)
{
	// 判断文件是否打开
	if ( m_file == NULL )
	{
		// 文件未打开
		return 2;
	}

	// 生成数据
	BuildFrameString(dt, net_buffer_size, file_str);

	// 写入文件
	fwrite(file_str, lstrlenA(file_str), 1, m_file);

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          将战场基本信息转化为 ACMI 格式的字符串。
*   @details        将战场基本信息转化为 ACMI 格式的字符串。
*   @param[in]      header          战场基本信息，详见 ACMI 格式说明。
*   @param[in]      string_length   字符串长度。
*   @param[out]     string          字符串，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*/
int TacViewFile_T::BuildString(
	const Header_T &header,
	const int string_length,
	char* string)
{
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	// 构建数据内容
	snprintf(file_str_line, sizeof(file_str_line), "FileType=text/acmi/tacview\n");
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "FileVersion=2.2\n");
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	/*snprintf(file_str_line, sizeof(file_str_line), "0,DataSource=%s\n", header.data_source);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,DataRecorder=%s\n", header.data_recorder);
	strcat_s(string, sizeof(char) * string_length, file_str_line);*/
	snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceTime=%s\n", header.reference_time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,RecordingTime=%s\n", header.recording_time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	/*snprintf(file_str_line, sizeof(file_str_line), "0,Author=%s\n", header.author);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Title=%s\n", header.title);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Category=%s\n", header.category);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Briefing=%s\n", header.briefing);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Debriefing=%s\n", header.debriefing);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Comments=%s\n", header.comments);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceLongitude=%.7f\n", header.reference_longitude);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceLatitude=%.7f\n", header.reference_latitude);
	strcat_s(string, sizeof(char) * string_length, file_str_line);*/

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          将仿真推进内容转化为 ACMI 格式的字符串。
*   @details        将仿真推进内容转化为 ACMI 格式的字符串。
*   @param[in]      state           仿真推进内容，详见 ACMI 格式说明。
*   @param[in]      string_length   字符串长度。
*   @param[out]     string          字符串，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*/
int TacViewFile_T::BuildString(
	const State_T &state,
	const int string_length,
	char* string)
{

	if (state.object_count == 0)
		return -1;

	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	// 构建数据内容
	snprintf(file_str_line, sizeof(file_str_line), "#%.2f\n", state.time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	for (int index = 0; index < state.object_count && index < max_object; index++)
	{
		if ( !state.object[index].isVisable )
		{
			snprintf(file_str_line, sizeof(file_str_line), "%d,Visible=0,LockedTarget=0\n", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
		else if ( state.object[index].live == 1 && state.object[index].id > 0 )
		{
			// id
			snprintf(file_str_line, sizeof(file_str_line), "%d,", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);

			if ( state.object[index].isRejoin )
			{
				snprintf(file_str_line, sizeof(file_str_line), "Visible=1,");
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}

			// 坐标
			if ( state.object[index].coordinate_valid == 1 )
			{
				snprintf(file_str_line, sizeof(file_str_line), "T=%.7f|%.7f|%.2f", state.object[index].coordinate_longitude, state.object[index].coordinate_latitude, state.object[index].coordinate_altitude);
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				switch ( state.object[index].coordinate_type )
				{
					// LLA
				case 0:
				{
					break;
				}

				// LLAUV
				case 1:
				{
					snprintf(file_str_line, sizeof(file_str_line), "|%.2f|%.2f", state.object[index].coordinate_u, state.object[index].coordinate_v);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					break;
				}

				// LLARPY
				case 2:
				{
					snprintf(file_str_line, sizeof(file_str_line), "|%.1f|%.1f|%.1f", state.object[index].coordinate_roll, state.object[index].coordinate_pitch, state.object[index].coordinate_yaw);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					break;
				}

				// LLARPYUVH
				case 3:
				{
					snprintf(file_str_line, sizeof(file_str_line), "|%.1f|%.1f|%.1f|%.2f|%.2f|%.2f", state.object[index].coordinate_roll, state.object[index].coordinate_pitch, state.object[index].coordinate_yaw, state.object[index].coordinate_u, state.object[index].coordinate_v, state.object[index].coordinate_heading);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					break;
				}

				// 错误
				default:
				{
					break;
				}
				}
			}

			// 基本信息
			if ( state.object[index].base_valid == 1 && state.object[index].isInit )
			{
				if ( strlen(state.object[index].base_name) > 0 )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",Name=%s", state.object[index].base_name);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
				if ( strlen(state.object[index].base_type) > 0 )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",Type=%s", state.object[index].base_type);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
				if ( strlen(state.object[index].base_pilot) > 0 )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",Pilot=%s", state.object[index].base_pilot);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
				if ( strlen(state.object[index].base_label) > 0 )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",Label=%s", state.object[index].base_label);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
				/*snprintf(file_str_line, sizeof(file_str_line), ",Country=%s", state.object[index].base_country);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",Coalition=%s", state.object[index].base_coalition);
				strcat_s(string, sizeof(char) * string_length, file_str_line);*/
				if ( strlen(state.object[index].base_color) > 0 )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",Color=%s", state.object[index].base_color);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
				/*snprintf(file_str_line, sizeof(file_str_line), ",Label=%s", state.object[index].base_label);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",Length=%.2f", state.object[index].base_length);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",Width=%.2f", state.object[index].base_width);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",Height=%.2f", state.object[index].base_height);
				strcat_s(string, sizeof(char) * string_length, file_str_line);*/

				snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				if ( SHOW_RADAR_ALL_RANGE )
					snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_all_range);
				else
					snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=0");
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				if ( SHOW_RADAR_ALL_HBEAMWIDTH )
					snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_all_hBeamWidth);
				else
					snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=0");
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				if ( SHOW_RADAR_ALL_VBEAMWIDTH )
					snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_all_vBeamWidth);
				else
					snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=0");
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				if ( SHOW_RADAR_ALL_RANGE )
					snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_all_azimuth);
				else
					snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=0");
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				if ( SHOW_RADAR_ALL_RANGE )
					snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_all_elevation);
				else
					snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=0");
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateMax=%.2f", state.object[index].radar_range);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateVerticalBeamwidth=%.2f", state.object[index].radar_vertical_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}

			//// 状态
			//if (state.object[index].state_valid == 1)
			//{
			//	snprintf(file_str_line, sizeof(file_str_line), ",FocusTarget=%d", state.object[index].state_focus_target);
			//	strcat_s(string, sizeof(char) * string_length, file_str_line);
			//	snprintf(file_str_line, sizeof(file_str_line), ",Radius=%.2f", state.object[index].state_radius);
			//	strcat_s(string, sizeof(char) * string_length, file_str_line);
			//	snprintf(file_str_line, sizeof(file_str_line), ",EngagementRange=%.2f", state.object[index].state_engagement_range);
			//	strcat_s(string, sizeof(char) * string_length, file_str_line);
			//}

			// 雷达
			if ( 1 == state.object[index].radar_valid )
			{
				/*snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_azimuth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_elevation);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_range);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_vertical_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);*/
				
				if ( state.object[index].isRadarModeChange )
				{
					snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					if ( SHOW_RADAR_ALL_RANGE )
						snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_all_range);
					else
						snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=0");
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					if ( SHOW_RADAR_ALL_HBEAMWIDTH )
						snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_all_hBeamWidth);
					else
						snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=0");
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					if ( SHOW_RADAR_ALL_VBEAMWIDTH )
						snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_all_vBeamWidth);
					else
						snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=0");
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					if ( SHOW_RADAR_ALL_RANGE )
						snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_all_azimuth);
					else
						snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=0");
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					if ( SHOW_RADAR_ALL_RANGE )
						snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_all_elevation);
					else
						snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=0");
					strcat_s(string, sizeof(char) * string_length, file_str_line);

					snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateMax=%.2f", state.object[index].radar_range);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateVerticalBeamwidth=%.2f", state.object[index].radar_vertical_beamwidth);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}

				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateAzimuth=%.2f", state.object[index].radar_azimuth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateElevation=%.2f", state.object[index].radar_elevation);
				strcat_s(string, sizeof(char)* string_length, file_str_line);

			}

			// 锁定目标
			if ( state.object[index].locked_target_valid == 1 )
			{
				snprintf(file_str_line, sizeof(file_str_line), ",LockedTarget=%d", state.object[index].locked_target_id);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				/*snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetMode=%d", state.object[index].locked_target_mode);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetAzimuth=%.2f", state.object[index].locked_target_azimuth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetElevation=%.2f", state.object[index].locked_target_elevation);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetRange=%.2f", state.object[index].locked_target_range);
				strcat_s(string, sizeof(char) * string_length, file_str_line);*/
			}

			snprintf(file_str_line, sizeof(file_str_line), "\n");
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
		else if ( state.object[index].live == 0 && state.object[index].id > 0 )
		{
			//id
			snprintf(file_str_line, sizeof(file_str_line), "-%d\n", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
	}

	return 0;
}

int TacViewFile_T::BuildRadarString(
	const State_T& state,
	const int string_length,
	char* string)
{
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	// 构建数据内容
	for ( int index = 0; index < state.object_count && index < max_object; index++ )
	{
		if ( state.object[index].live == 1 )
		{
			// id
			snprintf(file_str_line, sizeof(file_str_line), "%d,", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
			// 雷达
			if ( 1 == state.object[index].radar_valid )
			{
				snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			if ( SHOW_RADAR_ALL_RANGE )
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_all_range);
			else
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=0");
			strcat_s(string, sizeof(char) * string_length, file_str_line);

			if ( SHOW_RADAR_ALL_HBEAMWIDTH )
				snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_all_hBeamWidth);
			else
				snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=0");
			strcat_s(string, sizeof(char) * string_length, file_str_line);

			if ( SHOW_RADAR_ALL_VBEAMWIDTH )
				snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_all_vBeamWidth);
			else
				snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=0");
			strcat_s(string, sizeof(char) * string_length, file_str_line);

			if ( SHOW_RADAR_ALL_RANGE )
				snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_all_azimuth);
			else
				snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=0");
			strcat_s(string, sizeof(char) * string_length, file_str_line);

			if ( SHOW_RADAR_ALL_RANGE )
				snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_all_elevation);
			else
				snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=0");
			strcat_s(string, sizeof(char) * string_length, file_str_line);

				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateAzimuth=%.2f", state.object[index].radar_azimuth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateElevation=%.2f", state.object[index].radar_elevation);
				strcat_s(string, sizeof(char)* string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateMax=%.2f", state.object[index].radar_range);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateVerticalBeamwidth=%.2f\n", state.object[index].radar_vertical_beamwidth);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}
		}
	}
	return 0;
}

int TacView::TacViewFile_T::BuildBoundaryString(const State_T& state, const int string_length, char* string)
{
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	for ( int i = 0; i < max_boundary; i++ )
	{
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			state.boundaryPoint[i]._id,
			state.boundaryPoint[i]._longitude,
			state.boundaryPoint[i]._latitude,
			state.boundaryPoint[i]._altitude,
			state.boundaryPoint[i]._color
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);
	}
	
	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[0]._id, state.boundaryPoint[1]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[1]._id, state.boundaryPoint[2]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[2]._id, state.boundaryPoint[3]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[3]._id, state.boundaryPoint[0]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[4]._id, state.boundaryPoint[5]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[5]._id, state.boundaryPoint[6]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[6]._id, state.boundaryPoint[7]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", state.boundaryPoint[7]._id, state.boundaryPoint[4]._id);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	return 0;
}

int TacView::TacViewFile_T::BuildPathPlanString(const State_T& state, const int string_length, char* string)
{
	static std::map<int, unsigned int> PathTomonitorPointIDMap;
	extern const char* ColorString[7];
	static unsigned int lastNum = 0;
	static unsigned int reuseModelNum = 999;

	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	if ( state.pathPlan._isReuseModel )
	{
		for (int i = 0; i < Path_Count; i++)
		{
			if ( !state.pathPlan._enable || i >= Path_Count )
			{
				snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,Visible=0\n",
					reuseModelNum,
					state.pathPlan._planeID,
					i
				);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				continue;
			}

			if (i < state.pathPlan._pathCount && state.pathPlan._enable)
			{
				snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s,Visible=1\n",
					reuseModelNum,
					state.pathPlan._planeID,
					i,
					state.pathPlan._longitude[i],
					state.pathPlan._latitude[i],
					state.pathPlan._altitude[i],
					ColorString[state.pathPlan._color[i]]
				);
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				//snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,LockedTarget=%u%ld%d\n", lastNum, state.pathPlan._planeID, i, lastNum, state.pathPlan._planeID, i + 1);
				snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,FocusedTarget=%u%ld%d\n", reuseModelNum, state.pathPlan._planeID, i, reuseModelNum, state.pathPlan._planeID, i + 1);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}
		}
	}
	else
	{
		lastNum++;

		for (int i = 0; i < Path_Count; i++)
		{
			if ( lastNum > 1 )
			{
				snprintf(file_str_line, sizeof(file_str_line), "%u%d,Visible=0\n",
					PathTomonitorPointIDMap[state.pathPlan._planeID],
					i
				);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}

			if (i < state.pathPlan._pathCount && state.pathPlan._enable)
			{
				snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s,Visible=1\n",
					lastNum,
					state.pathPlan._planeID,
					i,
					state.pathPlan._longitude[i],
					state.pathPlan._latitude[i],
					state.pathPlan._altitude[i],
					ColorString[state.pathPlan._color[i]]
				);
				strcat_s(string, sizeof(char) * string_length, file_str_line);

			//if ( i == 0 )
			//{
			//	//snprintf(file_str_line, sizeof(file_str_line), "%d,LockedTarget=%u%ld%d\n", state.pathPlan._planeID, lastNum, state.pathPlan._planeID, i);
			//	snprintf(file_str_line, sizeof(file_str_line), "%d,FocusedTarget=%u%ld%d\n", state.pathPlan._planeID, lastNum, state.pathPlan._planeID, i);
			//	strcat_s(string, sizeof(char) * string_length, file_str_line);
			//}
			//snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,LockedTarget=%u%ld%d\n", lastNum, state.pathPlan._planeID, i, lastNum, state.pathPlan._planeID, i + 1);
			snprintf(file_str_line, sizeof(file_str_line), "%u%ld%d,FocusedTarget=%u%ld%d\n", lastNum, state.pathPlan._planeID, i, lastNum, state.pathPlan._planeID, i + 1);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
	}

		char temp[16] = { 0 };
		snprintf(temp, 16, "%u%ld", lastNum, state.pathPlan._planeID);
		PathTomonitorPointIDMap[state.pathPlan._planeID] = atoi(temp);
	}

	return 0;
}

int TacViewFile_T::BuildFrameString(
	const double dt,
	const int string_length,
	char* string)
{
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	// 构建数据内容
	snprintf(file_str_line, sizeof(file_str_line), "#%.3f\n", dt);
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	return 0;
}
