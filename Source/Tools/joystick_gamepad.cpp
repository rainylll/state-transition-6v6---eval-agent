#include "joystick_gamepad.h"
#include"interface.h"

#define range_joystic 65535
#define Fire 2
#define Lock 18000
#define Open_the_radar 128

static JOYINFO_OUTPUT joyOutput;

static int lastButt_state = 0;
static int lastPOV_state = range_joystic;
/**
*   @brief          初始化摇杆返回字与类型
*   @param[in]      dwSize            摇杆消息大小
*   @param[in]      dwFlags           摇杆控制字类型
*   @retval         void
*/
void JoystickInit(JOYINFOEX* joy)
{
	if (joy->dwSize == 0)
	{
		joy->dwSize = sizeof(JOYINFOEX);
		joy->dwFlags = JOY_RETURNALL;
	}
}
/**
*   @brief          获取摇杆消息
*   @param[in]      pji            摇杆类指针
*   @retval         void
*/
bool getJoystick(JOYINFOEX* pji)
{
	UINT joyNums;
	joyNums = joyGetNumDevs();
	if (joyNums >= 1)
	{
		if (JOYERR_NOERROR == joyGetPosEx(JOYSTICKID1, pji))
		{
			return true;
		}
	}

	return false;
}
/**
*   @brief          摇杆量到控制量转换
*   @param[in]      type            摇杆控制类型转换
*   @param[in]      range_begin     摇杆量转换下限
*   @param[in]      range_end       摇杆量转换上限
*   @retval         void
*/
void trans(JOYINFOEX *pji, int type, double range_begin = 0, double range_end = 0)
{
     	double Xpos = pji->dwXpos;
	    double Ypos = pji->dwYpos;
		double x = range_begin + (Xpos * (range_end - range_begin) / range_joystic );
		joyOutput.dwXpos = x;
	//	cout << joyOutput.dwXpos << endl;
		double y = range_begin + (Ypos * (range_end - range_begin) / range_joystic );
		joyOutput.dwYpos = y;
	//	cout << joyOutput.dwYpos << endl;
		joyOutput.dwButtons = pji->dwButtons;
		joyOutput.dwPOV = pji->dwPOV;
		joyOutput.dwZpos = (int(pji->dwZpos)*-100/ range_joystic)+100;
		//cout << joyOutput.dwZpos << endl;
//		cout << joyOutput.dwPOV << endl;
}

int Joystick_OutPut(JOYINFOEX *pji, void* data,double dt)
{
	if (!getJoystick(pji))
		return -1;

	PlaneState_S* planes = (PlaneState_S*)data;
	trans(pji, 1, -2.5, 2.5);
	//cout << planes[0]._roll_ctrl << endl;
	if (fabs(joyOutput.dwXpos) > 0.5)
	{
		planes[0]._roll_ctrl = fabs(pow(joyOutput.dwXpos, 2)) / 8;
		planes[0]._roll += joyOutput.dwXpos;
	}
	if (fabs(joyOutput.dwYpos) > 0.5)
	{
		planes[0]._pitch_ctrl = fabs(pow(joyOutput.dwYpos, 2)) / 20;
		planes[0]._pitch += joyOutput.dwYpos;
	}
	if (lastButt_state == 0&& joyOutput.dwButtons == Open_the_radar)
	{
		planes[0]._radarState = !bool(planes[0]._radarState);
	}
	if (joyOutput.dwPOV == Lock)
	{
		planes[0]._targetID = 20010;
		planes[0]._radarState = 2;
		//cout << planes[0]._raderState << endl;
	}
	if (lastButt_state == 0 && joyOutput.dwButtons == Fire)
	{
		planes[0]._isShoot = true;
	}
	planes[0]._throttle = joyOutput.dwZpos;
	lastPOV_state = joyOutput.dwPOV;
	lastButt_state = joyOutput.dwButtons;

	return joyOutput.dwButtons;
}
