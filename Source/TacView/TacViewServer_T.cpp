// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @file           TacViewServer_T.cpp
*   @brief          TacView 服务器。
*   @details        TacView 服务器，支持多个客户端连入。
*   @author         GaoYang
*   @date           20181126
*   @version        1.0.0.1
*   @par Copyright
*                   GaoYang
*   @par History
*                   1.0.0.1: GaoYang, 20181126, 首次创建
*                   1.0.0.2: lidaiwei, 20200808, 修正bug
*/

#include "TacViewServer_T.h"
#include "../FlyTac/aircraft.h"

#include <windows.h>
#include <codecvt>

#pragma comment(lib, "WS2_32.lib")

using namespace TacView;

extern bool SHOW_RADAR_ALL_HBEAMWIDTH;
extern bool SHOW_RADAR_ALL_VBEAMWIDTH;
extern bool SHOW_RADAR_ALL_RANGE;
extern bool SHOW_TACVIEW_RADAR;

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @name           静态变量。
*   @{
*/
bool							TacViewServer_T::m_open = false;
HANDLE							TacViewServer_T::m_idle = NULL;
SOCKET							TacViewServer_T::m_server_socket = NULL;
HANDLE							TacViewServer_T::m_listen_thread = NULL;

std::list<int>					TacViewServer_T::m_client_id_idle_list;
std::list<int>					TacViewServer_T::m_client_id_work_list;
SOCKET							TacViewServer_T::m_client_socket_list[4];
HANDLE							TacViewServer_T::m_recv_thread_list[4];
char							TacViewServer_T::m_recv_buffer_list[4][max_str];
HANDLE							TacViewServer_T::m_send_thread = NULL;
HANDLE							TacViewServer_T::m_send_message_nonempty = NULL;
std::list<std::string>			TacViewServer_T::m_send_message_list;
std::string						TacViewServer_T::m_send_history;
char                            TacViewServer_T::file_str[net_buffer_size];
/** @}  */

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          构造函数。
*   @details        构造函数，创建时默认被执行的函数。
*/
TacViewServer_T::TacViewServer_T()
{
	m_open = false;
	m_idle = NULL;
	m_server_socket = NULL;
	m_listen_thread = NULL;

	m_client_id_idle_list.clear();
	m_client_id_work_list.clear();;
	for ( int client_id = 0; client_id < 4; client_id++ )
	{
		m_client_id_idle_list.push_back(client_id);
		m_client_socket_list[client_id] = NULL;
		m_recv_thread_list[client_id] = NULL;
		memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
	}
	m_send_thread = NULL;
	m_send_message_nonempty = NULL;
	m_send_message_list.clear();
	m_send_history.clear();
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          析构函数。
*   @details        析构函数，销毁时默认被执行的函数。
*/
TacViewServer_T::~TacViewServer_T()
{
	m_open = false;
	m_idle = NULL;
	m_server_socket = NULL;
	m_listen_thread = NULL;

	m_client_id_idle_list.clear();
	m_client_id_work_list.clear();;
	for ( int client_id = 0; client_id < 4; client_id++ )
	{
		m_client_id_idle_list.push_back(client_id);
		m_client_socket_list[client_id] = NULL;
		m_recv_thread_list[client_id] = NULL;
		memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
	}
	m_send_thread = NULL;
	m_send_message_nonempty = NULL;
	m_send_message_list.clear();
	m_send_history.clear();
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          服务器打开函数。
*   @details        服务器打开函数。
*   @param[in]      port            端口号，默认 42674 。
*   @param[in]      header          战场基本信息，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*   @retval         2               错误 已经打开
*   @retval         3               错误 WSAStartup 失败
*   @retval         4               错误 Socket 初始化失败
*   @retval         5               错误 Socket 绑定失败
*   @retval         6               错误 Socket 监听失败
*/
int TacViewServer_T::Open(
	const int port,
	const Header_T& header)
{
	int return_code = 0;

	// 检查是否已经打开
	if ( m_open == true )
	{
		// 已经打开
		return 2;
	}

	// 变量初始化
	m_idle = ::CreateEvent(NULL, FALSE, TRUE, NULL);

	m_server_socket = NULL;
	m_listen_thread = NULL;
	m_client_id_idle_list.clear();
	m_client_id_work_list.clear();
	for ( int client_id = 0; client_id < 4; client_id++ )
	{
		m_client_id_idle_list.push_back(client_id);
		m_client_socket_list[client_id] = NULL;
		m_recv_thread_list[client_id] = NULL;
		memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
	}
	m_send_thread = NULL;
	m_send_message_nonempty = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	m_send_message_list.clear();
	m_send_history.clear();

	// 生成数据
	::WaitForSingleObject(m_idle, INFINITE);


	BuildString(header, net_buffer_size, file_str);
	std::string send_message = file_str;
	m_send_history += send_message;

	// 开启网络服务
	WSADATA wsa_data;

	return_code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if ( return_code != 0 )
	{
		WSACleanup();

		// WSAStartup 失败
		return 3;
	}

	m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if ( m_server_socket == INVALID_SOCKET )
	{
		WSACleanup();

		// Socket 初始化失败
		return 4;
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	return_code = bind(m_server_socket, (SOCKADDR*)&addr, sizeof(SOCKADDR));
	if ( return_code != 0 )
	{
		closesocket(m_server_socket);
		WSACleanup();

		// Socket 绑定失败
		return 5;
	}

	return_code = listen(m_server_socket, 1);
	if ( return_code != 0 )
	{
		closesocket(m_server_socket);
		WSACleanup();

		// Socket 监听失败
		return 6;
	}

	// 开启监听线程
	unsigned long thread_address;
	m_listen_thread = (HANDLE)::CreateThread(NULL, 0, &Listen, (void*)this, 0, &thread_address);

	// 开启数据发送处理线程
	m_send_thread = (HANDLE)::CreateThread(NULL, 0, &SendAll, (void*)this, 0, &thread_address);

	::SetEvent(m_idle);

	// 改变打开状态
	m_open = true;

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          服务器关闭函数。
*   @details        服务器关闭函数。
*   @retval         0               正常
*   @retval         1               错误
*   @retval         2               错误 未打开
*/
int TacViewServer_T::Close()
{
	int return_code = 0;

	// 检查是否未打开
	if ( m_open == false )
	{
		// 未打开
		return 2;
	}

	// 关闭发送线程
	::TerminateThread(m_send_thread, 0);
	m_listen_thread = NULL;
	::CloseHandle(m_send_message_nonempty);
	m_send_message_nonempty = NULL;
	m_send_message_list.clear();
	m_send_history.clear();

	// 关闭接收线程
	while ( m_client_id_work_list.empty() == false )
	{
		int client_id = m_client_id_work_list.front();
		m_client_id_work_list.pop_front();

		closesocket(m_client_socket_list[client_id]);
		m_client_socket_list[client_id] = NULL;

		::TerminateThread(m_recv_thread_list[client_id], 0);
		m_recv_thread_list[client_id] = NULL;

		memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));

		m_client_id_idle_list.push_back(client_id);
	}
	m_client_id_idle_list.clear();
	m_client_id_work_list.clear();

	// 关闭监听线程
	::TerminateThread(m_listen_thread, 0);
	m_listen_thread = NULL;

	// 关闭服务器
	return_code = closesocket(m_server_socket);
	m_server_socket = NULL;
	return_code = WSACleanup();

	::CloseHandle(m_idle);

	// 改变打开状态
	m_open = false;

	//delete[] file_str;

	return 0;
}

bool TacView::TacViewServer_T::isOpen()
{
	return m_open;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          向所有客户端发送仿真记录。
*   @details        向所有客户端发送仿真记录。
*   @param[in]      state           仿真推进内容，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*/
int TacViewServer_T::Send(
	const State_T& state)
{
	// 生成数据
	BuildString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	//--------------------
	/*static std::ofstream outfile("d:/example.txt");
	if ( outfile.is_open() ) {
		outfile <send_message< send_message << std::endl;
	}*/
	//--------------------

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          向所有客户端发送雷达数据。
*   @param[in]      state           仿真推进内容，详见 ACMI 格式说明。
*   @retval         0               正常
*   @retval         1               错误
*/
int TacViewServer_T::SendRadar(const State_T& state)
{
	// 生成数据
	BuildRadarString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

int TacView::TacViewServer_T::SendAddCircle(const State_T& state)
{
	// 生成数据
	BuildAddCircleString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

int TacView::TacViewServer_T::SendRemoveCircle(const State_T& state)
{
	// 生成数据
	BuildRemoveCircleString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

int TacView::TacViewServer_T::SendBoundary(const State_T& state)
{
	// 生成数据
	BuildBoundaryString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

int TacView::TacViewServer_T::SendDrawCrossGrid(const State_T& state)
{
	// 生成数据
	BuildCrossGridString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

int TacView::TacViewServer_T::SendPathPlan(const State_T& state)
{
	// 生成数据
	BuildPathPlanString(state, net_buffer_size, file_str);
	std::string send_message = file_str;

	::WaitForSingleObject(m_idle, INFINITE);

	// 加入发送队列
	m_send_message_list.push_back(send_message);
	::SetEvent(m_send_message_nonempty);

	// 保存历史消息
	m_send_history += send_message;

	::SetEvent(m_idle);

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          监听。
*   @details        监听，对连入的客户端进行管理。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::Listen(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	while ( 1 )
	{
		sockaddr_in sock_client;
		int sock_client_len = sizeof(SOCKADDR);

		SOCKET socket_client = accept(m_server_socket, (SOCKADDR*)&sock_client, &sock_client_len);

		if ( socket_client != INVALID_SOCKET )
		{
			// 有新连接客户端
			::WaitForSingleObject(m_idle, INFINITE);

			// 开启接收线程
			if ( m_client_id_idle_list.empty() != true )
			{
				int client_id = m_client_id_idle_list.front();
				m_client_id_idle_list.pop_front();

				switch ( client_id )
				{
				case 0:
				{
					unsigned long thread_address;
					HANDLE receive_thread = (HANDLE)::CreateThread(NULL, 0, &Receive_0, NULL, 0, &thread_address);

					m_client_id_work_list.push_back(client_id);
					m_client_socket_list[client_id] = socket_client;
					m_recv_thread_list[client_id] = receive_thread;
					memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));

					break;
				}

				case 1:
				{
					unsigned long thread_address;
					HANDLE receive_thread = (HANDLE)::CreateThread(NULL, 0, &Receive_1, NULL, 0, &thread_address);

					m_client_id_work_list.push_back(client_id);
					m_client_socket_list[client_id] = socket_client;
					m_recv_thread_list[client_id] = receive_thread;
					memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));

					break;
				}

				case 2:
				{
					unsigned long thread_address;
					HANDLE receive_thread = (HANDLE)::CreateThread(NULL, 0, &Receive_2, NULL, 0, &thread_address);

					m_client_id_work_list.push_back(client_id);
					m_client_socket_list[client_id] = socket_client;
					m_recv_thread_list[client_id] = receive_thread;
					memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));

					break;
				}

				case 3:
				{
					unsigned long thread_address;
					HANDLE receive_thread = (HANDLE)::CreateThread(NULL, 0, &Receive_3, NULL, 0, &thread_address);

					m_client_id_work_list.push_back(client_id);
					m_client_socket_list[client_id] = socket_client;
					m_recv_thread_list[client_id] = receive_thread;
					memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));

					break;
				}

				default:
				{
					closesocket(socket_client);
					break;
				}
				}
			}
			else
			{
				closesocket(socket_client);
			}

			// 发送握手数据
			char file_str_line[max_str];
			snprintf(file_str_line, sizeof(file_str_line), "XtraLib.Stream.0\nTacview.RealTimeTelemetry.0\ncasia\n");
			int send_data_size = send(socket_client, file_str_line, strlen(file_str_line) + 1, 0);

			// 发送历史数据
			send_data_size = send(socket_client, m_send_history.c_str(), m_send_history.length(), 0);

			::SetEvent(m_idle);
		}
		else
		{
			int return_code = WSAGetLastError();
			return_code = 0;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          接收数据。
*   @details        接收数据，每个客户端都在独立线程中运行。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::Receive_0(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	int client_id = 0;

	bool connect = true;
	while ( connect )
	{
		int recv_data_size = 0;
		recv_data_size = recv(m_client_socket_list[client_id], m_recv_buffer_list[client_id], max_str, 0);
		if ( recv_data_size <= 0 )
		{
			::WaitForSingleObject(m_idle, INFINITE);
			m_client_id_idle_list.push_back(client_id);
			closesocket(m_client_socket_list[client_id]);
			m_recv_thread_list[client_id] = NULL;
			memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
			::SetEvent(m_idle);

			connect = false;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          接收数据。
*   @details        接收数据，每个客户端都在独立线程中运行。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::Receive_1(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	int client_id = 1;

	bool connect = true;
	while ( connect )
	{
		int recv_data_size = 0;
		recv_data_size = recv(m_client_socket_list[client_id], m_recv_buffer_list[client_id], max_str, 0);
		if ( recv_data_size <= 0 )
		{
			::WaitForSingleObject(m_idle, INFINITE);
			m_client_id_idle_list.push_back(client_id);
			closesocket(m_client_socket_list[client_id]);
			m_recv_thread_list[client_id] = NULL;
			memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
			::SetEvent(m_idle);

			connect = false;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          接收数据。
*   @details        接收数据，每个客户端都在独立线程中运行。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::Receive_2(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	int client_id = 2;

	bool connect = true;
	while ( connect )
	{
		int recv_data_size = 0;
		recv_data_size = recv(m_client_socket_list[client_id], m_recv_buffer_list[client_id], max_str, 0);
		if ( recv_data_size <= 0 )
		{
			::WaitForSingleObject(m_idle, INFINITE);
			m_client_id_idle_list.push_back(client_id);
			closesocket(m_client_socket_list[client_id]);
			m_recv_thread_list[client_id] = NULL;
			memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
			::SetEvent(m_idle);

			connect = false;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          接收数据。
*   @details        接收数据，每个客户端都在独立线程中运行。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::Receive_3(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	int client_id = 3;

	bool connect = true;
	while ( connect )
	{
		int recv_data_size = 0;
		recv_data_size = recv(m_client_socket_list[client_id], m_recv_buffer_list[client_id], max_str, 0);
		if ( recv_data_size <= 0 )
		{
			::WaitForSingleObject(m_idle, INFINITE);
			m_client_id_idle_list.push_back(client_id);
			closesocket(m_client_socket_list[client_id]);
			m_recv_thread_list[client_id] = NULL;
			memset(m_recv_buffer_list[client_id], 0, sizeof(m_recv_buffer_list[client_id]));
			::SetEvent(m_idle);

			connect = false;
		}
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          接收数据。
*   @details        接收数据，每个客户端都在独立线程中运行。
*   @param[in]      para            多线程注册函数格式要求，创建线程时可传入参数。
*   @retval         0               正常
*   @retval         1               错误
*/
unsigned long __stdcall TacViewServer_T::SendAll(
	void* para)
{
	UNREFERENCED_PARAMETER(para);

	while ( 1 )
	{
		::WaitForSingleObject(m_send_message_nonempty, INFINITE);

		::WaitForSingleObject(m_idle, INFINITE);

		while ( m_send_message_list.empty() == false )
		{
			std::string send_message = m_send_message_list.front();
			m_send_message_list.pop_front();

			for ( std::list<int>::iterator client_id_iterator = m_client_id_work_list.begin(); client_id_iterator != m_client_id_work_list.end(); client_id_iterator++ )
			{
				int client_id = *client_id_iterator;

				send(m_client_socket_list[client_id], send_message.c_str(), send_message.length(), 0);

				//std::wstring send_message_utf8;
				//std::wstring_convert<std::codecvt_utf8<wchar_t>> convert_utf8;
				//send_message_utf8 = convert_utf8.from_bytes(send_message.c_str());

				//send(m_client_socket_list[client_id], (char*)send_message_utf8.c_str(), send_message_utf8.length(), 0);
			}

		}

		::SetEvent(m_idle);
	}

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
int TacViewServer_T::BuildString(
	const Header_T& header,
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
	snprintf(file_str_line, sizeof(file_str_line), "0,DataSource=%s\n", header.data_source);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,DataRecorder=%s\n", header.data_recorder);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceTime=%s\n", header.reference_time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,RecordingTime=%s\n", header.recording_time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,Author=%s\n", header.author);
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
	/*snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceLongitude=%.7f\n", header.reference_longitude);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	snprintf(file_str_line, sizeof(file_str_line), "0,ReferenceLatitude=%.7f\n", header.reference_latitude);*/
	strcat_s(string, sizeof(char) * string_length, file_str_line);

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
int TacViewServer_T::BuildString(
	const State_T& state,
	const int string_length,
	char* string)
{
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	// 构建数据内容
	snprintf(file_str_line, sizeof(file_str_line), "#%.2f\n", state.time);
	strcat_s(string, sizeof(char) * string_length, file_str_line);
	for ( int index = 0; index < state.object_count && index < max_object; index++ )
	{
		if ( !state.object[index].isVisable )
		{
			snprintf(file_str_line, sizeof(file_str_line), "%d,Visible=0,LockedTarget=0\n", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
		else if ( state.object[index].live == 1 )
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
				if ( strlen(state.object[index].base_name) > 0)
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
				strcat_s(string, sizeof(char) * string_length, file_str_line);*/
				/*snprintf(file_str_line, sizeof(file_str_line), ",Coalition=%s", state.object[index].base_coalition);
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
			}

			// 状态
			if (state.object[index].state_valid == 1)
			{
				snprintf(file_str_line, sizeof(file_str_line), ",FocusTarget=%d", state.object[index].state_focus_target);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",Radius=%.2f", state.object[index].state_radius);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				snprintf(file_str_line, sizeof(file_str_line), ",EngagementRange=%.2f", state.object[index].state_engagement_range);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
			}

			// 雷达
			if ( 1 == state.object[index].radar_valid )
			{
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_azimuth);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_elevation);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_range);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_vertical_beamwidth);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);

				if (SHOW_TACVIEW_RADAR)
				{
					if ( state.object[index].isRadarModeChange )
					{
						snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_all_range);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_HBEAMWIDTH)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_all_hBeamWidth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_VBEAMWIDTH)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_all_vBeamWidth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_all_azimuth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
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
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
			}

			// 锁定目标
			if (state.object[index].locked_target_valid == 1)
			{
				snprintf(file_str_line, sizeof(file_str_line), ",LockedTarget=%d", state.object[index].locked_target_id);
				strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetMode=%d", state.object[index].locked_target_mode);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetAzimuth=%.2f", state.object[index].locked_target_azimuth);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetElevation=%.2f", state.object[index].locked_target_elevation);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
				//snprintf(file_str_line, sizeof(file_str_line), ",LockedTargetRange=%.2f", state.object[index].locked_target_range);
				//strcat_s(string, sizeof(char) * string_length, file_str_line);
			}

			if ( state.object[index].focused_target_valid == 1 )
			{
				snprintf(file_str_line, sizeof(file_str_line), ",FocusedTarget=%d", state.object[index].focusedTarget);
				strcat_s(string, sizeof(char)* string_length, file_str_line);
			}

			snprintf(file_str_line, sizeof(file_str_line), "\n");
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}
		else
		{
			//id
			snprintf(file_str_line, sizeof(file_str_line), "-%d\n", state.object[index].id);
			strcat_s(string, sizeof(char) * string_length, file_str_line);
		}

	}

	for ( int index = 0; index < state.event_count && index < max_event; index++ )
	{
		snprintf(file_str_line, sizeof(file_str_line), "0,Event=%s|%d|%s", state.event[index].type,
			state.event[index].id_list[0],
			state.event[index].text);
		strcat_s(string, sizeof(char) * string_length, file_str_line);
	}
	snprintf(file_str_line, sizeof(file_str_line), "\n");
	strcat_s(string, sizeof(char) * string_length, file_str_line);

	return 0;
}

int TacViewServer_T::BuildRadarString(
	const State_T& state,
	const int string_length,
	char* string)
{
	if (SHOW_TACVIEW_RADAR)
	{

		// 初始化数据
		memset(string, 0, sizeof(char) * string_length);

		char file_str_line[max_str];
		memset(file_str_line, 0, sizeof(file_str_line));

		// 构建数据内容
		snprintf(file_str_line, sizeof(file_str_line), "#%.2f\n", state.time);
		strcat_s(string, sizeof(char) * string_length, file_str_line);
		for (int index = 0; index < state.object_count && index < max_object; index++)
		{
			if (state.object[index].live == 1)
			{
				// id
				snprintf(file_str_line, sizeof(file_str_line), "%d,", state.object[index].id);
				strcat_s(string, sizeof(char) * string_length, file_str_line);

				// 雷达
				if ( 1 == state.object[index].radar_valid )
				{
					if ( state.object[index].isRadarModeChange )
					{
						snprintf(file_str_line, sizeof(file_str_line), ",RadarMode=%d", state.object[index].radar_mode);
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=%.2f", state.object[index].radar_all_range);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarRange=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_HBEAMWIDTH)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=%.2f", state.object[index].radar_all_hBeamWidth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarHorizontalBeamwidth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_VBEAMWIDTH)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=%.2f", state.object[index].radar_all_vBeamWidth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarVerticalBeamwidth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=%.2f", state.object[index].radar_all_azimuth);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarAzimuth=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						if (SHOW_RADAR_ALL_RANGE)
							snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=%.2f", state.object[index].radar_all_elevation);
						else
							snprintf(file_str_line, sizeof(file_str_line), ",RadarElevation=0");
						strcat_s(string, sizeof(char) * string_length, file_str_line);

						snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateMax=%.2f", state.object[index].radar_range);
						strcat_s(string, sizeof(char) * string_length, file_str_line);
						snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateHorizontalBeamwidth=%.2f", state.object[index].radar_horizontal_beamwidth);
						strcat_s(string, sizeof(char) * string_length, file_str_line);
						snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateVerticalBeamwidth=%.2f\n", state.object[index].radar_vertical_beamwidth);
						strcat_s(string, sizeof(char) * string_length, file_str_line);
					}

					snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateAzimuth=%.2f", state.object[index].radar_azimuth);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
					snprintf(file_str_line, sizeof(file_str_line), ",RadarRangeGateElevation=%.2f", state.object[index].radar_elevation);
					strcat_s(string, sizeof(char) * string_length, file_str_line);
				}
			}
		}
	}
	return 0;
}

int TacView::TacViewServer_T::BuildAddCircleString(const State_T& state, const int string_length, char* string)
{
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	char colorStr[color_size] = {0};
	switch ( state.circleData.color )
	{
	case Color_E::Red:
		strncpy(colorStr, "Red", strlen("Red"));
		break;
	case Color_E::Orange:
		strncpy(colorStr, "Orange", strlen("Orange"));
		break;
	case Color_E::Yellow:
		strncpy(colorStr, "Yellow", strlen("Yellow"));
		break;
	case Color_E::Green:
		strncpy(colorStr, "Green", strlen("Green"));
		break;
	case Color_E::Cyan:
		strncpy(colorStr, "Cyan", strlen("Cyan"));
		break;
	case Color_E::Violet:
		strncpy(colorStr, "Violet", strlen("Violet"));
		break;
	case Color_E::Blue:
		strncpy(colorStr, "Blue", strlen("Blue"));
		break;
	}
	
	snprintf(file_str_line, sizeof(file_str_line), "%d,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s,EngagementRange=%d\n",
		state.circleData.cid,
		state.circleData.longitude,
		state.circleData.latitude,
		state.circleData.altitude,
		colorStr,
		state.circleData.radius
	);

	strcat_s(string, sizeof(char) * string_length, file_str_line);
	return 0;
}

int TacView::TacViewServer_T::BuildRemoveCircleString(const State_T& state, const int string_length, char* string)
{
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	snprintf(file_str_line, sizeof(file_str_line), "%d,Visible=0\n",
		state.circleData.cid
	);

	strcat_s(string, sizeof(char) * string_length, file_str_line);
	return 0;
}

int TacView::TacViewServer_T::BuildBoundaryString(const State_T& state, const int string_length, char* string)
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

std::map<int, std::pair<double, double>> centerPointMap;
int centerPointMapIndex;

int TacView::TacViewServer_T::BuildCrossGridString(const State_T& state, const int string_length, char* string)
{
	centerPointMapIndex = 0;
	centerPointMap.clear();
	static int CrossGridId = 15000;
	// 初始化数据
	memset(string, 0, sizeof(char) * string_length);

	char file_str_line[max_str];
	memset(file_str_line, 0, sizeof(file_str_line));

	int baseId = CrossGridId;
	std::pair<double, double> baseRight    = LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 90, state.crossGrid.childSize);
	std::pair<double, double> baseBottom   = LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 180, state.crossGrid.childSize);
	std::pair<double, double> baseDiagonal = LongLatOffset(baseBottom.first, baseBottom.second, 90, state.crossGrid.childSize);
	char colorStr[color_size] = {0};
	switch ( state.crossGrid.color )
	{
	case Color_E::Red:
		strncpy(colorStr, "Red", strlen("Red"));
		break;
	case Color_E::Orange:
		strncpy(colorStr, "Orange", strlen("Orange"));
		break;
	case Color_E::Yellow:
		strncpy(colorStr, "Yellow", strlen("Yellow"));
		break;
	case Color_E::Green:
		strncpy(colorStr, "Green", strlen("Green"));
		break;
	case Color_E::Cyan:
		strncpy(colorStr, "Cyan", strlen("Cyan"));
		break;
	case Color_E::Violet:
		strncpy(colorStr, "Violet", strlen("Violet"));
		break;
	case Color_E::Blue:
		strncpy(colorStr, "Blue", strlen("Blue"));
		break;
	}

	int hcc		= state.crossGrid.hNum % 2 == 0 ? state.crossGrid.hNum / 2 : state.crossGrid.hNum / 2 + 1;
	int upNum	= state.crossGrid.hNum / 2;
	upNum		= upNum >= hcc ? upNum - 1 : upNum;
	int downNum = state.crossGrid.hNum / 2;
	
	for ( int i = 1; i <= state.crossGrid.hNum; i++ )
	{
		std::pair<double, double> topLeft;
		std::pair<double, double> topRight;
		std::pair<double, double> topDiagonal;
		std::pair<double, double> topBottom;

		if (i < hcc)
		{
			topLeft		= LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 0, upNum-- * state.crossGrid.childSize);
			topRight	= LongLatOffset(topLeft.first, topLeft.second, 90, state.crossGrid.childSize);
			topDiagonal = LongLatOffset(topRight.first, topRight.second, 180, state.crossGrid.childSize);
			topBottom	= LongLatOffset(topDiagonal.first, topDiagonal.second, 270, state.crossGrid.childSize);
		}
		else if (i == hcc)
		{
			topLeft.first	= state.crossGrid.baseLongitude;
			topLeft.second	= state.crossGrid.baseLatitude;
			topRight		= baseRight;
			topDiagonal		= baseDiagonal;
			topBottom		= baseBottom;
		}
		else if (i > hcc)
		{
			topLeft		= LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 180, downNum-- * state.crossGrid.childSize);
			topRight	= LongLatOffset(topLeft.first, topLeft.second, 90, state.crossGrid.childSize);
			topDiagonal = LongLatOffset(topRight.first, topRight.second, 180, state.crossGrid.childSize);
			topBottom	= LongLatOffset(topDiagonal.first, topDiagonal.second, 270, state.crossGrid.childSize);
		}
		else
			continue;

		double d1 = GetDistance(topLeft.first, topLeft.second, topRight.first, topRight.second) / 2;
		double d2 = GetDistance(topBottom.first, topBottom.second, topDiagonal.first, topDiagonal.second) / 2;
		
		std::pair<double, double> cp1 = LongLatOffset(topLeft.first, topLeft.second, 90, d1);
		std::pair<double, double> cp2 = LongLatOffset(cp1.first, cp1.second, 180, d2);
		centerPointMap[centerPointMapIndex++] = cp2;

		int leftId = CrossGridId;

		//left
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topLeft.first,
			topLeft.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//right
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topRight.first,
			topRight.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//diagonal
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topDiagonal.first,
			topDiagonal.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//bottom
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topBottom.first,
			topBottom.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, leftId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);
	}

	int vcc		 = state.crossGrid.hNum % 2 == 0 ? state.crossGrid.hNum / 2 : state.crossGrid.hNum / 2 + 1;
	int leftNum  = state.crossGrid.vNum / 2;
	leftNum		 = leftNum >= vcc ? leftNum - 1 : leftNum;
	int rightNum = state.crossGrid.vNum / 2;

	for ( int i = 1; i <= state.crossGrid.vNum; i++ )
	{
		std::pair<double, double> topLeft;
		std::pair<double, double> topRight;
		std::pair<double, double> topDiagonal;
		std::pair<double, double> topBottom;

		if (i < vcc)
		{
			topLeft = LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 270, leftNum-- * state.crossGrid.childSize);
			topRight = LongLatOffset(topLeft.first, topLeft.second, 90, state.crossGrid.childSize);
			topDiagonal = LongLatOffset(topRight.first, topRight.second, 180, state.crossGrid.childSize);
			topBottom = LongLatOffset(topDiagonal.first, topDiagonal.second, 270, state.crossGrid.childSize);
		}
		else if (i == vcc)
		{
			continue;
		}
		else if (i > vcc)
		{
			topLeft = LongLatOffset(state.crossGrid.baseLongitude, state.crossGrid.baseLatitude, 90, rightNum-- * state.crossGrid.childSize);
			topRight = LongLatOffset(topLeft.first, topLeft.second, 90, state.crossGrid.childSize);
			topDiagonal = LongLatOffset(topRight.first, topRight.second, 180, state.crossGrid.childSize);
			topBottom = LongLatOffset(topDiagonal.first, topDiagonal.second, 270, state.crossGrid.childSize);
		}
		else
			continue;

		double d1 = GetDistance(topLeft.first, topLeft.second, topRight.first, topRight.second) / 2;
		double d2 = GetDistance(topBottom.first, topBottom.second, topDiagonal.first, topDiagonal.second) / 2;

		std::pair<double, double> cp1 = LongLatOffset(topLeft.first, topLeft.second, 90, d1);
		std::pair<double, double> cp2 = LongLatOffset(cp1.first, cp1.second, 180, d2);
		centerPointMap[centerPointMapIndex++] = cp2;

		int leftId = CrossGridId;

		//left
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topLeft.first,
			topLeft.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//right
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topRight.first,
			topRight.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//diagonal
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topDiagonal.first,
			topDiagonal.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, CrossGridId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		//bottom
		snprintf(file_str_line, sizeof(file_str_line), "%ld,T=%f|%f|%f,Type=Navaid+Static+Waypoint,Name=1000 gal Drop Tank,Color=%s\n",
			CrossGridId++,
			topBottom.first,
			topBottom.second,
			state.crossGrid.baseAltitude,
			colorStr
		);
		strcat_s(string, sizeof(char) * string_length, file_str_line);

		snprintf(file_str_line, sizeof(file_str_line), "%ld,LockedTarget=%d\n", CrossGridId - 1, leftId);
		strcat_s(string, sizeof(char) * string_length, file_str_line);
	}

	return 0;
}

int TacView::TacViewServer_T::BuildPathPlanString(const State_T& state, const int string_length, char* string)
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
