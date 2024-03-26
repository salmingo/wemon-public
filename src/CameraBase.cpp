
#include <boost/date_time/posix_time/posix_time.hpp>
#include "CameraBase.h"
#include "GLog.h"

using namespace boost;
using namespace boost::posix_time;

CameraBase::CameraBase() {
}

CameraBase::~CameraBase() {
	Disconnect();
}

void CameraBase::RegisterExpose(const CBSlot& slot) {
	cbfExpose_.disconnect_all_slots();
	cbfExpose_.connect(slot);
}

bool CameraBase::Connect() {
	if (!info_.connected && open_camera()) {
		info_.connected = true;
		info_.state     = CAMERA_IDLE;
		info_.errcode   = CAMEC_SUCCESS;
		info_.pixels = info_.wSensor * info_.hSensor;
		info_.Alloc();
		thrdExpose_.reset(new boost::thread(boost::bind(&CameraBase::thread_expose, this)));
		thrdTemp_.reset(new boost::thread(boost::bind(&CameraBase::thread_temperature, this)));

		return true;
	}
	return false;
}

void CameraBase::Disconnect() {
	if (info_.connected) {
		cooler_onoff(false, 0);
		interrupt_thread(thrdExpose_);
		interrupt_thread(thrdTemp_);
		if (info_.state == CAMERA_EXPOSE) AbortExpose();
		close_camera();
		info_.Reset();
	}
}

void CameraBase::CoolerOnoff(bool onoff, int coolerSet) {
	if (info_.connected) cooler_onoff(onoff, coolerSet);
}

bool CameraBase::Expose(double expdur, bool light) {
	if (info_.state == CAMERA_IDLE) {
		set_ShtrMode(light ? 0 : 2);
		if (fabs(expdur - info_.expdur) > 1E-3 && !set_expdur(expdur)) {
			info_.state   = CAMERA_ERROR;
			info_.errcode = CAMEC_FAIL_EXPDUR;
		}
		else if (start_expose()) {
			info_.expdur = expdur;
			cvExpBegin_.notify_one();
			return true;
		}
		else {
			info_.state = CAMERA_ERROR;
			info_.errcode = CAMEC_FAIL_EXPOSE;
		}
	}
	return false;
}

bool CameraBase::AbortExpose() {
	if (info_.state == CAMERA_EXPOSE && stop_expose()) {
		while (info_.state == CAMERA_EXPOSE) boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		info_.capturing = false;
		return true;
	}
	return false;
}

bool CameraBase::SetROI(int &x0, int &y0, int &w, int &h, int xbin, int ybin) {
	if (info_.state != CAMERA_IDLE) return false;
	if (xbin < 1 || xbin > info_.wSensor) return false;
	if (ybin < 1 || ybin > info_.hSensor) return false;
	int res;
	if (xbin != 1 && (res = x0 % xbin) != 1) x0 -= (res - 1);
	if (ybin != 1 && (res = y0 % ybin) != 1) y0 -= (res - 1);
	if (xbin != 1 && (res = w % xbin) != 0) w -= res;
	if (ybin != 1 && (res = h % ybin) != 0) h -= res;
	if ((res = x0 + w - info_.wSensor - 1) > 0) w -= res;
	if ((res = y0 + h - info_.hSensor - 1) > 0) h -= res;
	if (w <= 0 || h <= 0) return false;

	if (set_ROI(x0, y0, w, h, xbin, ybin)) {
		info_.pixels = w * h / xbin / ybin;
		info_.xorgin = x0;
		info_.yorgin = y0;
		info_.width  = w;
		info_.height = h;
		info_.xbin   = xbin;
		info_.ybin   = ybin;

		info_.useROI = xbin != 1 || ybin != 1 || w != info_.wSensor || h != info_.hSensor;
		return true;
	}
	return false;
}

bool CameraBase::SetADChannel(uint16_t index) {
	if (info_.state == CAMERA_IDLE && index != info_.iADChannel
			&& set_ADChannel(index, info_.bitdepth)) {
		info_.iADChannel = index;
		return true;
	}
	return false;
}

bool CameraBase::SetReadPort(uint16_t index) {
	if (info_.state == CAMERA_IDLE && index != info_.iReadport
			&& set_ReadPort(index, info_.readport)) {
		info_.iReadport = index;
		return true;
	}
	return false;
}

bool CameraBase::SetReadRate(uint16_t index) {
	if (info_.state == CAMERA_IDLE && index != info_.iReadrate
			&& set_ReadRate(index, info_.readrate)) {
		info_.iReadrate = index;
		return true;
	}
	return false;
}

bool CameraBase::SetPreampGain(uint16_t index) {
	if (info_.state == CAMERA_IDLE && index != info_.iPreampGain
			&& set_gain_preamp(index, info_.gainPreamp)) {
		info_.iPreampGain = index;
		return true;
	}
	return false;
}

bool CameraBase::SetVerticalShift(uint16_t index) {
	if (info_.state == CAMERA_IDLE && index != info_.iVerShift
			&& set_vershift(index, info_.verShiftRate)) {
		info_.iVerShift = index;
		return true;
	}
	return false;
}

bool CameraBase::SetEMGain(bool onoff, uint16_t gain) {
	if (info_.state == CAMERA_IDLE && info_.EMSupport
			&& set_gain_em(onoff, gain)) {
		info_.EMON   = onoff;
		info_.EMGain = gain;
		return true;
	}
	return false;
}

bool CameraBase::set_ShtrMode(int mode) {
	if (!info_.hasShutter || mode == info_.shtrMode) return true;
	return false;
}

void CameraBase::thread_expose() {
	chrono::seconds toWait(1);
	mutex mtx;
	MtxLck lck(mtx);
	int& state = info_.state;
	double percent(0), left(0);
	ptime tmNow;

	while (1) {
		cvExpBegin_.wait(lck); // 等待新的曝光

		info_.dateobs = microsec_clock::universal_time();
		state = CAMERA_EXPOSE;
		while (state == CAMERA_EXPOSE && cvExpOver_.wait_for(lck, toWait) == cv_status::timeout) {// 监测曝光过程
			tmNow = microsec_clock::universal_time();
			left  = info_.expdur - (tmNow - info_.dateobs).total_microseconds() * 1E-6;
			if (left < 1E-6) left = 0;
			if (info_.expdur < 1E-6) percent = 100.0001;
			else percent = (1.0 - left / info_.expdur) * 100.0001;
			cbfExpose_(state, percent, left);
		}
		/* state==
			* CAMERA_IDLE   -- 停止曝光, 不读出并存储图像文件
			* CAMERA_IMGRDY -- 结束曝光, 读出并存储文件
			* CAMERA_ERROR  -- 故障, 依据故障字处理
			*/
		if (state == CAMERA_IMGRDY) info_.dateend = microsec_clock::universal_time();
		cbfExpose_(state, 100, 0);
		if (state == CAMERA_IMGRDY) state = CAMERA_IDLE;
	}
}

void CameraBase::thread_temperature() {
	boost::chrono::seconds toWait(1);

	while (1) {
		boost::this_thread::sleep_for(toWait);
		if (info_.state == CAMERA_IDLE && !sensor_temperature(info_.coolGet)) {
			if (++info_.errcnt > 3) {
				info_.errcode = CAMEC_GET_TEMP;
				info_.state = CAMERA_ERROR;
			}
		}
		else if (info_.state == CAMERA_ERROR && sensor_temperature(info_.coolGet)) {
			if (info_.errcode == CAMEC_GET_TEMP) {
				info_.state = CAMERA_IDLE;
				info_.errcnt = CAMEC_SUCCESS;
			}
		}
	}
}
