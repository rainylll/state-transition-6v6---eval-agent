#include "../Tools/interface.h"
#include "../Tools/WaypointControl.h"

#include <iomanip>

#define _PLANECOUNT		4

int main()
{
	cout << fixed << setprecision(6);

	PlaneState_S planes[_PLANECOUNT];
	memset(planes, 0, sizeof(PlaneState_S));
	planes[0].init(MAV_RED_1);
	planes[0]._planeID = 10010;
	planes[0]._longitude = 118.663042;
	planes[0]._latitude = 23.577106;
	planes[0]._altitude = 10000;
	planes[0]._yaw = 270;
	planes[0]._roll = 0;
	planes[0]._pitch = 0;
	planes[0]._team = 1;
	planes[0]._targetID = -1;
	planes[0]._raderState = 1;
	planes[0]._roll_ctrl = 0.2;
	planes[0]._pitch_ctrl = 0.2;
	planes[0]._yaw_ctrl = 0.2;
	planes[0]._throttle = 20;

	InitEnv(0, planes, _PLANECOUNT);

	while (EnvStep(0.1, planes))
	{
		RadarStep(6);

		const PlaneState_S* obj = GetPlaneState(10010);

		memcpy(planes, obj, sizeof(PlaneState_S));
		
		cout << "飞机ID："  << planes[0]._planeID	<< "|"
			 << "经度："	<< planes[0]._longitude << "|"
			 << "纬度："	<< planes[0]._latitude	<< "|"
			 << "高度："	<< planes[0]._altitude	<< "|"
			 << "真空速："	<< planes[0].TAS		<< "|"
			 << "过载："	<< planes[0]._overload  << endl;

		Sleep(100);
	}

	return 0;
}