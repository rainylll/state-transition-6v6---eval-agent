#include "extern.h"

using namespace std;

const pair<double, double> left_down{ 120, 23 };

Point::Point()
{

}
Point::Point(double x, double y, double z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

Point::Point(double x, double y, double z, double azimuth, double pitch, double v)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->azimuth = azimuth;
	this->pitch = pitch;
	this->v = v;
}
Point::Point(double x, double y, double z, double azimuth, double v)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->azimuth = azimuth;
	this->v = v;
}
Point::Point(double x, double y, double z, double azimuth, double v, float V_cmd, float H_cmd, float A_cmd)
{
	this->x = x;
	this->y = y;
	this->z = z;
	this->azimuth = azimuth;
	this->v = v;
	this->V_cmd = V_cmd;
	this->H_cmd = H_cmd;
	this->A_cmd = A_cmd;
}
Point::Point(int planeID, double x, double y, double z, double azimuth, double pitch, double roll, double v, float V_cmd, float H_cmd, float A_cmd)
{
	this->planeID = planeID;
	this->x = x;
	this->y = y;
	this->z = z;
	this->roll = roll;
	this->pitch = pitch;
	this->azimuth = azimuth;
	this->v = v;
	this->V_cmd = V_cmd;
	this->H_cmd = H_cmd;
	this->A_cmd = A_cmd;
}




double DisOfTwoPoint(double slog, double slat, double elog, double elat)
{
	double x1, x2, y_1, y2, z1, z2, alfa1, seit1, alfa2,
		seit2, beit, x, distance = 0.0, temppara;

	if (slog > 179.999 || slog < 0.001 || slat>70.0 || slat < 0.001 || elog>179.999 || elog < 0.001 || elat>70.0 || elat < 0.001)
		return 0.0;
	alfa1 = slog;
	seit1 = 90.0 - slat;
	alfa2 = elog;
	seit2 = 90.0 - elat;

	x1 = EARTH_RADIUS * sin(seit1 * GR_to_R) * cos(alfa1 * GR_to_R);
	y_1 = EARTH_RADIUS * sin(seit1 * GR_to_R) * sin(alfa1 * GR_to_R);
	z1 = EARTH_RADIUS * cos(seit1 * GR_to_R);

	x2 = EARTH_RADIUS * sin(seit2 * GR_to_R) * cos(alfa2 * GR_to_R);
	y2 = EARTH_RADIUS * sin(seit2 * GR_to_R) * sin(alfa2 * GR_to_R);
	z2 = EARTH_RADIUS * cos(seit2 * GR_to_R);

	temppara = pow(x2 - x1, 2.0) + pow(y2 - y_1, 2.0) + pow(z2 - z1, 2.0);
	if (temppara < 0)
		temppara = 0;
	x = sqrt(temppara);

	x = x / 2 / EARTH_RADIUS;
	beit = asin(x);

	beit *= R_to_GR;
	beit *= 2;

	distance = 2 * PI * EARTH_RADIUS * beit / 360.0;

	return distance;
}

float Cal_Heading_T(double Long, double Lat, double long_T, double lat_T)
{
	double  heading_T;
	double	DLong, DLat;
	double  distance_to_target;
	double  sinheading;

	DLong = long_T - Long;
	DLat = lat_T - Lat;

	distance_to_target = DisOfTwoPoint(Long, Lat, long_T, lat_T);

	if (distance_to_target < 200.)
	{

		if (fabs(DLat) < 0.00001)
		{
			if (DLong > 0)
				return 90.;
			else
				return 270.;
		}

		heading_T = atan2(DLong * cos(lat_T * GR_to_R), DLat) * R_to_GR;
		return (float)heading_T;
	}

	sinheading = sin(fabs(DLong) * GR_to_R) * sin((90 - lat_T) * GR_to_R) / sin(distance_to_target / EARTH_RADIUS);

	if (sinheading > 1)
	{
		sinheading = 1;
	}
	else if (sinheading < -1)
	{
		sinheading = -1;
	}

	if (DLong >= 0 && DLat >= 0)		//1
	{
		heading_T = asin(sinheading) * R_to_GR;
	}
	else if (DLong < 0 && DLat >= 0)		//2
	{
		heading_T = -asin(sinheading) * R_to_GR;
	}
	else if (DLong < 0 && DLat < 0)		//3
	{
		heading_T = -(90. + acos(sinheading) * R_to_GR);
	}
	else							//4
	{
		heading_T = 90. + acos(sinheading) * R_to_GR;
	}
	if (heading_T < 0.0)
		heading_T += 360.0;
	else if (heading_T > 360.0)
		heading_T -= 360.0;
	return (float)heading_T;
}

std::pair<double, double> CompPosition(float delta_de_inm, float delta_dn_inm, double longitude, double latitude)
{

	double distin1, distin2, theta, earthr, Long0, Lati0, Out_Long, Out_Lati;
	double a, b, temppara;
	Long0 = longitude;
	Lati0 = latitude;
	//if(fabs(delta_de_inm) > 20.0 || fabs(delta_dn_inm) > 20.0)
	//return;
	if (Long0 < 0.001 || Long0 > 179.999 || Lati0 < 0.001 || Lati0 > 70.0)
		return{ Long0, Lati0 };
	a = 6378136.0;
	b = 6356751.0;

	theta = atan(b * b * tan(Lati0 * PI / 180.0) / a / a);
	earthr = 1.0 / sqrt(cos(theta) * cos(theta) / a / a + sin(theta) * sin(theta) / b / b);
	distin1 = PI / 180.0 * earthr * cos(theta); //latitude纬度 一度代表的距离（米）
	temppara = sqrt((earthr / a * earthr / a - earthr / b * earthr / b) * (earthr / a * earthr / a - earthr / b * earthr / b) * (sin(2.0 * theta) / 2.0) * (sin(2.0 * theta) / 2.0) + 1.0);
	distin2 = temppara * earthr * PI / 180.0;
	Out_Long = Long0 + delta_de_inm / distin1;
	Out_Lati = atan(a / b * a / b * tan(theta + delta_dn_inm / distin2 * PI / 180.0)) / PI * 180.0;
	/**longitude = Out_Long;
	*latitude = Out_Lati;*/
	return std::make_pair(Out_Long, Out_Lati);
}

std::pair<double, double> generateLon_Lat(double s_lon, double s_lat, double angle, double distance)
{
	double re = EARTH_RADIUS * cos(s_lat * GR_to_R);
	double t_lon = s_lon + distance * sin(angle * GR_to_R) / (re * 2 * PI) * 360;
	double t_lat = s_lat + distance * cos(angle * GR_to_R) / (re * 2 * PI) * 360;
	return std::make_pair(t_lon, t_lat);
}

Point toXYZ(double lon, double lat, double h)
{
	double dist = DisOfTwoPoint(left_down.first, left_down.second, lon, lat);
	double angle = Cal_Heading_T(left_down.first, left_down.second, lon, lat);
	Point temp{ dist * sin(angle / 180 * PI), dist * cos(angle / 180 * PI), h };
	return temp;
}

double angleStandardization(double angle)
{
	while (angle < 0)
	{
		angle += 360;
	}
	return (int)angle % 360 + angle - (int)angle;
}
double angletorad(double angle)
{
	//转化成0-2pi
	angle = angleStandardization(angle);
	return (angle / 180) * PI;
}
double radtoangle(double rad)
{
	double angle;
	angle = (rad / PI) * 180;
	angle = fmod(angle, 360.0);    // 取模确保角度在 0 到 360 之间
	if (angle < 0) {
		angle += 360.0;            // 将负角度转为正角度
	}
	return angle;
}

double Angle_Range_conversion(double angle) //确保将角度转换为[-pi,pi)
{
	if (angle >= 180)
	{
		angle = angle - 360;
		angle = (angle / 180) * PI;
	}
	else
	{
		angle = (angle / 180) * PI;
	}
	return angle;
}

double Pinth_Angle_Range_conversion(double angle) //确保将角度转换为[-pi/2, pi/2)
{
	if (angle >= 90)
	{
		angle = angle - 180;
		angle = (angle / 180) * PI;
	}
	else
	{
		angle = (angle / 180) * PI;
	}
	return angle;
}

double Get_Desire_Height(const double Height) //获取阶段高度
{
	double Desire_Height = 0;

	if (Height < 4000)
	{
		Desire_Height = Height_Level5;
	}
	else if (Height >= 4000 && Height < 6000)
	{
		Desire_Height = Height_Level4;
		/*if (Height <= (Height_Level4 + Height_Level5) / 2)
		{
		Desire_Height = Height_Level5;
		}
		else if (Height >(Height_Level4 + Height_Level5) / 2)
		{
		Desire_Height = Height_Level4;
		}*/
	}
	else if (Height >= 6000 && Height < 8000)
	{
		Desire_Height = Height_Level3;
		/*if (Height <= (Height_Level3 + Height_Level4) / 2)
		{
		Desire_Height = Height_Level4;
		}
		else if (Height >(Height_Level3 + Height_Level4) / 2)
		{
		Desire_Height = Height_Level3;
		}*/
	}
	else
	{
		Desire_Height = Height_Level3;
	}

	//else if (Height >= 8000 && Height < 10000)
	//{

	//	Desire_Height = Height_Level2;
	//	/*if (Height <= (Height_Level2 + Height_Level3) / 2)
	//	{
	//	Desire_Height = Height_Level3;
	//	}
	//	else if (Height >(Height_Level2 + Height_Level3) / 2)
	//	{
	//	Desire_Height = Height_Level2;
	//	}*/
	//}
	//else if (Height >= 10000 && Height < 11000)
	//{
	//	Desire_Height = Height_Level1;
	//	/*if (Height < (Height_Level1 + Height_Level2) / 2)
	//	{
	//	Desire_Height = Height_Level2;
	//	}
	//	else if (Height >= (Height_Level1 + Height_Level2) / 2)
	//	{
	//	Desire_Height = Height_Level1;
	//	}*/
	//}
	//else if (Height >= 11000)
	//{
	//	Desire_Height = Height_Level1;
	//}

	return Desire_Height;
}
double Get_Desire_Speed(const double Velocity) //获取阶段速度
{
	double Desire_Speed = 0;
	if (Velocity >= (Speed_Level4 + Speed_Level5) / 2)
	{
		Desire_Speed = Speed_Level4;
	}
	else
	{
		Desire_Speed = Speed_Level5;
	}
	if (Velocity >= (Speed_Level3 + Speed_Level4) / 2)
	{
		Desire_Speed = Speed_Level3;
	}
	else
	{
		Desire_Speed = Speed_Level4;
	}
	if (Velocity >= (Speed_Level2 + Speed_Level3) / 2)
	{
		Desire_Speed = Speed_Level2;
	}
	else
	{
		Desire_Speed = Speed_Level3;
	}
	if (Velocity >= (Speed_Level1 + Speed_Level2) / 2)
	{
		Desire_Speed = Speed_Level1;
	}
	else
	{
		Desire_Speed = Speed_Level2;
	}
	return Desire_Speed;
}


bool isAngleEnough(double eDelta, double angle1, double angle2)
{
	angle1 = angle1 * GR_to_R;
	angle2 = angle2 * GR_to_R;
	double  angleRes = acos(sin(angle1)*sin(angle2) + cos(angle1)*cos(angle2))*R_to_GR;

	// cout <<"   "<< angle1 <<"     "<< angle2 <<"      "<< angleRes << endl;
	if (angleRes <= eDelta)
	{
		return true;
	}
	else
	{
		return false;
	}
}


double distance(double mine_x, double mine_y, double mine_z, double enemy_x, double enemy_y, double enemy_z)   //计算敌我距离
{
	double x_diff = abs(mine_x - enemy_x);
	double y_diff = abs(mine_y - enemy_y);
	double z_diff = abs(mine_z - enemy_z);
	return (sqrt(pow(x_diff, 2) + pow(y_diff, 2) + pow(z_diff, 2)));
}
double distance(double mine_x, double mine_y, double enemy_x, double enemy_y)   //计算敌我距离
{
	double x_diff = abs(mine_x - enemy_x);
	double y_diff = abs(mine_y - enemy_y);
	return (sqrt(pow(x_diff, 2) + pow(y_diff, 2)));
}

double isAngleEnough_off_axis(double angle1, double angle2)
{
	angle1 = angle1 * GR_to_R;
	angle2 = angle2 * GR_to_R;
	double  angleRes = acos(sin(angle1)*sin(angle2) + cos(angle1)*cos(angle2))*R_to_GR;

	// cout <<"   "<< angle1 <<"     "<< angle2 <<"      "<< angleRes << endl;

	return angleRes;

}