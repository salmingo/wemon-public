#include "CameraQHY.h"

CameraQHY::CameraQHY() {
	hcam_ = NULL;
}

CameraQHY::~CameraQHY() {
}

void CameraQHY::thread_wait_frame() {
	boost::mutex mtx;
	MtxLck lck(mtx);
	uint32_t w, h, bpp, channels(0);
	uint32_t rc;

	while (true) {
		cvWaitFrm_.wait(lck);
		rc = GetQHYCCDSingleFrame(hcam_, &w, &h, &bpp, &channels, info_.data.get());
		info_.state = rc == QHYCCD_SUCCESS ? CAMERA_IMGRDY : CAMERA_IDLE;
		if (info_.state == CAMERA_ERROR) info_.errcode = CAMEC_FAIL_READOUT;
		cvExpOver_.notify_one();
	}
}

bool CameraQHY::open_camera() {
	int num(0), found(0);
	char id[32], model[32];

//	EnableQHYCCDMessage(false);
//	EnableQHYCCDLogFile(false);

	if(QHYCCD_SUCCESS != InitQHYCCDResource()){
		info_.errcode = CAMEC_FAIL_INIT;
		return false;
	}
	if(0 == ScanQHYCCD()) {
		info_.errcode = CAMEC_NOT_FOUND;
		return false;
	}
	if (QHYCCD_SUCCESS == GetQHYCCDId(0, id))
		found = 1;
	if (!found) {
		info_.errcode = CAMEC_NOT_FOUND;
		return false;
	}
	else if (NULL == (hcam_ = OpenQHYCCD(id))
			|| (QHYCCD_SUCCESS != SetQHYCCDStreamMode(hcam_, 0))
			|| (QHYCCD_SUCCESS != InitQHYCCD(hcam_))) {
		info_.errcode = CAMEC_NOT_OPEN;
		if (hcam_) {
			CloseQHYCCD(hcam_);
			ReleaseQHYCCDResource();
			hcam_ = NULL;
		}
	}
	else {
		GetQHYCCDModel(id, model);
		info_.model = model;

		info_.iReadport   = 0;
		info_.readport    = "CMOS";
		info_.iReadrate   = 0;
		info_.readrate    = "USBRATE 0";
		info_.iPreampGain = 5;
		info_.gainPreamp  = 5;

		info_.iVerShift    = 0;
		info_.verShiftRate = 29.1;

		uint32_t bpp;			//< A/D转换数字位数
		double chipw, chiph;	//< 芯片物理尺寸, 量纲: mm
		double pixelw, pixelh;	//< 像元物理尺寸, 量纲: um
		GetQHYCCDChipInfo(hcam_, &chipw, &chiph, &info_.wSensor, &info_.hSensor, &pixelw, &pixelh, &bpp);
		info_.iADChannel = 0;
		info_.bitdepth   = bpp;
		info_.pixSizeX   = float(pixelw);
		info_.pixSizeY   = float(pixelh);

		if (info_.wSensor > 4000) SetQHYCCDParam(hcam_, CONTROL_GAIN, 5); // 4040
		else SetQHYCCDParam(hcam_, CONTROL_GAIN, 15);  // 533M

		SetQHYCCDParam(hcam_, CONTROL_OFFSET, 15);
		SetQHYCCDParam(hcam_, CONTROL_TRANSFERBIT, 16);
		SetQHYCCDParam(hcam_, CONTROL_SPEED, 0);
		SetQHYCCDParam(hcam_, CONTROL_DDR, 1);
		SetQHYCCDDebayerOnOff(hcam_, false);
		// QHY 533M需要设置ROI
		uint32_t xsensor_, ysensor_, wsensor_, hsensor_;
        GetQHYCCDEffectiveArea(hcam_, &xsensor_, &ysensor_, &wsensor_, &hsensor_);
        SetQHYCCDBinMode(hcam_, 1, 1);
        SetQHYCCDResolution(hcam_, xsensor_, ysensor_, wsensor_, hsensor_);

		info_.EMSupport  = false;
		info_.hasShutter = false;

		thrdWaitFrm_.reset(new boost::thread(boost::bind(&CameraQHY::thread_wait_frame, this)));
		return true;
	}

	return false;
}

void CameraQHY::close_camera() {
	interrupt_thread(thrdWaitFrm_);
	if (hcam_) CloseQHYCCD(hcam_);
	ReleaseQHYCCDResource();
	hcam_ = NULL;
}

void CameraQHY::cooler_onoff(bool onoff, int coolerSet) {
	if (onoff) {
		if (QHYCCD_SUCCESS == ControlQHYCCDTemp(hcam_, coolerSet)) {
			info_.coolOn = true;
			info_.coolSet = coolerSet;
		}
	}
	else {// 设置制冷功率
		SetQHYCCDParam(hcam_, CONTROL_MANULPWM, 0.0);
		info_.coolOn = false;
	}
}

bool CameraQHY::sensor_temperature(int& temperature) {
	temperature =  (int) GetQHYCCDParam(hcam_, CONTROL_CURTEMP);
	return temperature < 100;
}

bool CameraQHY::set_expdur(double expdur) {
	if (SetQHYCCDParam(hcam_, CONTROL_EXPOSURE, expdur * 1E6) != QHYCCD_SUCCESS)
		return false;
	info_.expdur = expdur;
	return true;
}

bool CameraQHY::start_expose() {
	if (QHYCCD_SUCCESS == ExpQHYCCDSingleFrame(hcam_)) {
		cvWaitFrm_.notify_one();
		return true;
	}
	return false;
}

bool CameraQHY::stop_expose() {
	return QHYCCD_SUCCESS == CancelQHYCCDExposingAndReadout(hcam_);
}

bool CameraQHY::set_ROI(int x0, int y0, int w, int h, int xbin, int ybin) {
	return (QHYCCD_SUCCESS == SetQHYCCDBinMode(hcam_, xbin, ybin)
		&& SetQHYCCDResolution(hcam_, x0, y0, w, h));
}

bool CameraQHY::set_ADChannel(uint16_t index, uint16_t &bitdepth) {
	bitdepth = 16;
	return true;
}

bool CameraQHY::set_ReadPort(uint16_t index, string& value) {
	value = "CMOS";
	return true;
}

bool CameraQHY::set_ReadRate(uint16_t index, string& value) {
	double vmin, vmax, step;
	if (QHYCCD_SUCCESS == GetQHYCCDParamMinMaxStep(hcam_, CONTROL_SPEED, &vmin, &vmax, &step)) {
		if (index != info_.iReadrate && index >= vmin && index <= vmax) {
			if (QHYCCD_SUCCESS == SetQHYCCDParam(hcam_, CONTROL_SPEED, index)) {
				value = "USBRATE " + std::to_string(index);
				return true;
			}
		}
	}
	return false;
}

bool CameraQHY::set_gain_preamp(uint16_t index, float& gain) {
	double vmin, vmax, step;
	if (QHYCCD_SUCCESS == GetQHYCCDParamMinMaxStep(hcam_, CONTROL_GAIN, &vmin, &vmax, &step)) {
		if (index != info_.iPreampGain && index >= vmin && index <= vmax) {
			if (QHYCCD_SUCCESS == SetQHYCCDParam(hcam_, CONTROL_GAIN, index)) {
				info_.gainPreamp = index;
				return true;
			}
		}
	}
	return false;
}

bool CameraQHY::set_vershift(uint16_t index, float& rate) {
	rate = info_.verShiftRate;
	return true;
}

bool CameraQHY::set_gain_em(bool onoff, uint16_t gain) {
	return false;
}

bool CameraQHY::init_parameters() {
	return true;
}

void CameraQHY::load_parameters() {
}
