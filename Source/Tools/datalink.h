#pragma once
#pragma warning(disable:4996)
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <utility>

#define TOPICNAME_SIZE	32
#define BUFF_SIZE		1024
typedef void (*RecvCallBack)(char*, int);

#pragma pack(1)
typedef struct
{
	int  dataSize;
	char topicName[TOPICNAME_SIZE];
	char data[BUFF_SIZE];
} TopicData_S;
#pragma pack()

class _declspec(dllimport) IDataLink
{
public:
	typedef enum
	{
		SERVER,
		CLIENT
	} ROLE;

	IDataLink(ROLE role);
	~IDataLink();

	void			wlog(const char* text);
	virtual bool	open(ROLE role);
	virtual int		setData(void* data, unsigned int size);
	virtual int		getData(void* data, unsigned int size);
	virtual int		peek(void* data, unsigned int size);
	virtual void	close();

protected:
	WSADATA						_wsaData;
	int							_socked;
	struct ip_mreq				_mreq;
	struct sockaddr_in			_remote_addr;
};

class _declspec(dllimport) DataLinkServer : public IDataLink
{
public:
	DataLinkServer();
	~DataLinkServer();

	bool startTopicServer();
	void stopTopicServer();
private:
	static unsigned int																_currentAddressCount;
	static int																		_topicRegisterServerFD;
	static int																		_topicHandleServerFD;
	static int																		_broadcastServerSocked;
	static bool																		_isOpen;
	static std::map<std::string, std::pair<std::string, std::string>> 				_topicMap;
	static std::map<std::string, std::vector<std::pair<std::string, short>>>		_topicClientsMap;

	HANDLE																			_hRegisterThread;
	HANDLE																			_hHandleThread;

	static DWORD WINAPI recvResiterThreadFunc(LPVOID lpParam);
	static DWORD WINAPI recvHandleThreadFunc(LPVOID lpParam);
	bool initBroadcastServer();
	void closeBroadcastServer();

};

class _declspec(dllimport) DataLinkClient : public IDataLink
{
public:
	DataLinkClient(const char* topicName, const char* serverHost = "127.0.0.1");
	~DataLinkClient();

	void outData();

	int sendData(const char* data, const int size);
	void setRecvData(char* data, const int size, RecvCallBack func);

private:
	static int			_topicServerFD;

	static int			_maxDataSize;
	static char* _data;
	struct ip_mreq		_mreq;
	HANDLE				_hThread;

	static int			_reisterServerFD;
	struct sockaddr_in	_reisterServerAddress;

	static int			_localServerFD;
	int					_handleServerFD;
	struct sockaddr_in	_handleServerAddress;

	std::string			_serverHost;
	std::string			_topicName;
	std::string			_host;
	std::string			_port;

	bool registerTopicName(const char* topicName, const char* serverHost = "127.0.0.1");
	bool bindResult();

	bool startTopicServer();
	void stopTopicServer();
	bool sendBroadcastData(std::string topicName, short port);

	static DWORD WINAPI recvThreadFunc(LPVOID lpParam);
	static RecvCallBack _recvCallBack;

};

