// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @file           tool_function.cpp
*   @brief          计算功能函数。
*   @details        常用的数学计算。
*   @author         LiDaiwei
*   @date           20191112
*   @version        1.0.0.1
*   @par Copyright
*                   LiDaiwei
*   @par History
*                   1.0.0.1: LiDaiwei, 20191112, 首次创建

*/


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @name           头文件。
*   @{
*/

#include "tool_function.h"

#include <algorithm>

/** @}  */


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          角度转弧度。
*   @details        角度转弧度。
*   @param[out]     radOut          弧度输出
*   @param[in]      degIn           角度输入
*   @retval         0               正常
*   @retval         1               错误
*/
int deg2rad(
	double* radOut,
	const double degIn)
{
	*radOut = M_PI*degIn/180;
	return 0;

}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          弧度转角度。
*   @details        弧度转角度。
*   @param[out]     degOut          弧度输出
*   @param[in]      radIn           角度输入
*   @retval         0               正常
*   @retval         1               错误
*/
int rad2deg(
	double* degOut,
	const double radIn)
{
	*degOut = 180*radIn/M_PI;
	return 0;
}


// --------------------------------------------------------------------------------------------------------------------------------
/**
*   @brief          生成高斯分布随机数序列。
*   @details        生成高斯分布随机数序列:期望为0.0，方差为1.0。
*   @param[out]     gaussOut        随机数输出
*   @retval         0               正常
*   @retval         1               错误
*/
int gaussrand(
	double* gaussOut
	)
{
    static double V1, V2, S;
    static int phase = 0;
    double X;

    if ( phase == 0 ) {
        do {
            double U1 = (double)rand() / RAND_MAX;
            double U2 = (double)rand() / RAND_MAX;

            V1 = 2 * U1 - 1;
            V2 = 2 * U2 - 1;
            S = V1 * V1 + V2 * V2;
        } while(S >= 1 || S == 0);

        X = V1 * sqrt(-2 * log(S) / S);
    } else
        X = V2 * sqrt(-2 * log(S) / S);

    phase = 1 - phase;

    *gaussOut = X;
    return 0;
}

int getYawPoint(double _yaw_original, double _yaw)
{
#define _diff _original360 - _yaw360
	double _original360 = _yaw_original;
	double _yaw360 = _yaw;

	if ( _yaw < 0 )
		_yaw360 = 360 - fabs(_yaw);
	if ( _yaw_original < 0 )
		_original360 = 360 - fabs(_yaw_original);

	if ( _yaw360 == 0 && _yaw_original != 0 )
		_yaw360 = 360;

	//printf("_yaw_original = %f\n", _yaw_original);
	//printf("_yaw = %f\n", _yaw);
	//printf("_original360 = %f\n", _original360);
	//printf("_yaw360 = %f\n", _yaw360);
	//printf("_diff = %f\n", _diff);

	if ( _diff > 0 )
	{
		if ( _yaw_original > 0 && _yaw > 0 )
		{
			//printf("111\n");
			return -1;
		}
		else if ( _yaw_original < 0 && _yaw < 0 )
		{
			//printf("222\n");
			return -1;
		}
		else if ( _yaw_original < 0 && _yaw > 0 )
		{
			//printf("333\n");
			return _diff < 180 ? -1 : 1;
		}
		else
		{
			//printf("444\n");
			return _diff < 180 ? 1 : -1;
		}
	}
	else
	{
		if ( _yaw_original > 0 && _yaw > 0 )
		{
			//printf("555\n");
			return 1;
		}
		else if ( _yaw_original < 0 && _yaw < 0 )
		{
			//printf("666\n");
			return 1;
		}
		else if ( _yaw_original < 0 && _yaw > 0 )
		{
			//printf("777\n");
			return _diff < 180 ? 1 : -1;
		}
		else
		{
			if ( _diff < 0 )
			{
				//printf("888\n");
				return fabs(_diff) < 180 ? 1 : -1;
			}
			else
			{
				//printf("999\n");
				return fabs(_diff) < 180 ? -1 : 1;
			}
		}
	}
}

double rad_to_angle(double rad)
{
	return rad * (180.0 / 3.14159265358979323846);
}