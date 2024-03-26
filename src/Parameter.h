
#ifndef _SRC_PARAMETER_H_
#define _SRC_PARAMETER_H_

#include <string>

using std::string;

struct Parameter {
public:
	Parameter();
	~Parameter();

public:
	bool Init(const char* filePath);
	bool Load(const char* filePath);
	bool Save(const char* filePath);

public:
	/* 设备编号 */
	string devID;		///< 设备编号
	/* 测站位置 */
	string siteName;	///< 测站名称
	double siteLon;		///< 测站地理经度, 角度
	double siteLat;		///< 测站地理维度, 角度
	double siteAlt;		///< 测站海拔

	/* 对外服务 */
	string addrMulticast;	///< 组播地址
	int portMulticast;		///< 组播端口
	int codeMulticast;		///< 组播信息编码格式: 1: JSON; 2: Struct - 西光
	int portCommand;		///< UDP服务: 响应控制指令

	bool enablePDXP;	///< 启用PDXP
	string addrPDXP;	///< PDXP协议主机地址
	int portPDXP;		///< PDXP协议主机端口
	string addrPDXP1;	///< PDXP协议主机地址
	int portPDXP1;		///< PDXP协议主机端口

	/* 智能PDU */
	string addrPDU;		///< PDU地址
	int portPDU;		///< PDU端口
	int portDevice;		///< 设备电源在PDU上的端口

	/* 采样周期 */
	int sampleCycle;	///< 采样周期, 秒. >= 10
	string sampleDir;	///< 测量数据存储目录

	/* 气象站 */
	string portWeaStation;	///< 气象站串口名称
	bool rainEnable;		///< 启用单独雨水
	string portRain;		///< 单独雨水串口名称

	/* SQM */
	bool sqmEnable;		///< 启用SQM
	string addrSQM;		///< SQM地址

	/* 云量相机 */
	string fileCloudAge;///< 云量分布交换文件
	string dirRawImage;	///< 相机文件存储根目录名称
	string prefixName;	///< 目录名前缀
	int sunEleMax;		///< 太阳仰角上限, 角度
	int expdurMin;		///< 最短曝光时间, 秒. >= 0
	int expdurMax;		///< 最大曝光时间, 秒
	int saturation;		///< 饱和值
	int coolerSet;		///< 制冷温度
	int minDiskFree;	///< 最小可用磁盘空间, GB
	double fwhmPerfect;	///< 期望FWHM值
};

#endif
