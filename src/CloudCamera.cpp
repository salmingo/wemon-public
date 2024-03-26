
#include <longnam.h>
#include <fitsio.h>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include "GLog.h"
#include "ADefine.h"
#include "CloudCamera.h"
#include "ProtoFocus.h"

#ifdef ENABLE_CAMERA
#include "CloudCamera/CameraQHY.h"
#endif

using namespace boost::posix_time;
using namespace boost::filesystem;
using namespace boost::placeholders;
using namespace AstroUtil;

#define FOCUS_FRAME_MAX		3
#define FOCUS_CONFIDENCE	0.1
#define FWHM_EXPECT			3.0
#define FWHM_EXPECT_ERROR	0.2

#define FOCUS_OVER		0
#define FOCUS_MANUAL	1
#define FOCUS_AUTO		2

CloudCamera::CloudCamera(const Parameter* param) {
    param_   = param;
    fpLog_   = NULL;
	focusMode_ = FOCUS_OVER;
	info_.state = WMC_FAIL_CONNECT;
}

CloudCamera::~CloudCamera() {
	Stop();
    _gLog.Write("Cloud Camera: stopped");
}

bool CloudCamera::Start() {
    try {
        ptime tmNow = second_clock::universal_time();
        ptime::date_type today = tmNow.date();

        // 测量记录
        path pathName(param_->sampleDir);
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
        }
		pathName /= param_->prefixName;
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
        }
        pathName /= (boost::format("Y%d") % today.year()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
        }
        pathName /= (boost::format("WMC_%d%02d%02d.log") % today.year() % today.month().as_number() % today.day()).str();
        path pathLog(pathName);

        // 原始图像
        pathName = param_->dirRawImage;
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
        }
        pathName /= (boost::format("%s%d%02d%02d") % param_->prefixName % (today.year() - 2000) % today.month().as_number() % today.day()).str();
        if (!exists(pathName)) {
            create_directory(pathName);
            permissions(pathName, perms::remove_perms | perms::group_write | perms::others_write);
        }
        dirRawImg_ = pathName.string();

        // 云图=>数据处理
        pathName = param_->sampleDir;
        pathName /= "observed.list";
        pathNtfyProc_ = pathName.string();
        if (exists(path(pathLog))) copy_file(pathLog, pathName, copy_options::overwrite_existing);
        else remove(pathName);
        // 打开日志文件
        fpLog_ = fopen(pathLog.c_str(), "a+");
    }
    catch(filesystem_error& ex) {
        _gLog.Write(LOG_FAULT, "[%s:%s], %s", __FILE__, __FUNCTION__, ex.what());
        return false;
    }

    thrdMain_.reset(new boost::thread(boost::bind(&CloudCamera::run, this)));
    return true;
}

void CloudCamera::Stop() {
    interrupt_thread(thrdMain_);
	interrupt_thread(thrdReduce_);
    if (camPtr_.unique()) {
        camPtr_->Disconnect();
        camPtr_.reset();
    }
    if (fpLog_) {
        fclose(fpLog_);
        fpLog_ = NULL;
    }
}

/**
 * @brief 注册调焦回调函数
 */
void CloudCamera::RegisterCBFocus(const CBSlot& slot) {
	cbfFocus_.disconnect_all_slots();
	cbfFocus_.connect(slot);
}

/**
 * @brief 启动/停止调焦流程
 * @param enable  启动标志
 */
void CloudCamera::DoFocus(UdpPtr udp, bool enable, bool manual) {
	if (!enable) {
		focusMode_ = FOCUS_OVER;
		interrupt_thread(thrdReduce_); // 中断调焦
		udpFocusPtr_.reset();
	}
	else if (!focusMode_) {// 启动调焦
		if (!manual) {
			focusAlgo_.Init(param_->fwhmPerfect, FWHM_EXPECT_ERROR);
			udpFocusPtr_ = udp;
		}
		focusMode_ = manual ? FOCUS_MANUAL : FOCUS_AUTO;
		queImg_.frames.clear();
		queFwhm_.clear();
		thrdReduce_.reset(new boost::thread(boost::bind(&CloudCamera::thread_reduce, this)));
	}
}

/**
 * @brief 最后一次调焦命令步长超出限位区域
 */
void CloudCamera::FocusTargetOverLimit() {
	//...
}

/**
 * @brief 命令调焦转到指定步长
 * @param step  > 0: 顺时针; < 0: 逆时针
 */
void CloudCamera::FocusMove(int step) {

}

void CloudCamera::expose_process(int state, double percent, double left) {
	if (state == CAMERA_IMGRDY && !cloud2fits()) {
		cloudadj(); // 评估图像中心区域亮度并调整曝光时间
		++frmno_;
	}
#ifdef NDEBUG
	if (state != CAMERA_EXPOSE) _gLog.Write("camera state = %d", state);
#endif
}

int CloudCamera::cloud2fits() {
	const CameraInfo* nfcam = camPtr_->GetInfo();
	fitsfile *fitsptr;
	int status(0);
	int naxis(2);
	long naxes[] = {nfcam->wSensor, nfcam->hSensor};
	long pixels = nfcam->pixels;
	// 生成文件名
	ptime::date_type dateobs = nfcam->dateobs.date();
	ptime::time_duration_type timeobs = nfcam->dateobs.time_of_day();
	ptime::time_duration_type timeend = nfcam->dateend.time_of_day();
	path filePath(dirRawImg_);
	boost::format fmtFileName("C%sT%02d%02d%02d.fit");

	fmtFileName % to_iso_string(nfcam->dateobs.date()) % timeobs.hours() % timeobs.minutes() % timeobs.seconds();
	filePath /= fmtFileName.str();

	// 存储FITS文件并写入完整头信息
	fits_create_file(&fitsptr, filePath.c_str(), &status);
	fits_create_img(fitsptr, USHORT_IMG, naxis, naxes, &status);
	fits_write_img(fitsptr, TUSHORT, 1, pixels, nfcam->data.get(), &status);

	/* FITS头 */
	string imgtype("OBJECT");
	fits_write_key(fitsptr, TSTRING, "CCDTYPE", (void*)imgtype.c_str(), "type of image", &status);
	fits_write_key(fitsptr, TSTRING, "DATE-OBS", (void*)to_iso_extended_string(dateobs).c_str(), "UTC date of begin observation", &status);
	fits_write_key(fitsptr, TSTRING, "TIME-OBS", (void*)to_simple_string(timeobs).c_str(), "UTC time of begin observation", &status);
	fits_write_key(fitsptr, TSTRING, "TIME-END", (void*)to_simple_string(timeend).c_str(), "UTC time of end observation", &status);

	double jd = dateobs.julian_day() + timeobs.total_seconds() / AU_DAYSEC - 0.5;
	fits_write_key(fitsptr, TDOUBLE, "JD", (void*)&jd, "Julian day of begin observation", &status);
	fits_write_key(fitsptr, TDOUBLE, "EXPTIME", (void*)&nfcam->expdur, "exposure duration", &status);
	fits_write_key(fitsptr, TFLOAT, "GAIN", (void*) &nfcam->gainPreamp, "preamp gain/index", &status);
	fits_write_key(fitsptr, TINT, "TEMPSET", (void*)&nfcam->coolSet, "cooler set point", &status);
	fits_write_key(fitsptr, TINT, "TEMPACT", (void*)&nfcam->coolGet, "cooler actual point", &status);

	string termtype("CloudCamera");
	fits_write_key(fitsptr, TSTRING, "TERMTYPE", (void*)termtype.c_str(), "terminal type", &status);

	int focus(12); // 12mm老蛙镜头
	fits_write_key(fitsptr, TINT, "TELFOCUS", &focus, "telescope focus value in micron", &status);
	fits_write_key(fitsptr, TINT, "FRAMENO", &frmno_, "frame no in this run", &status);

	fits_write_key(fitsptr, TSTRING, "DEVID",    (void*)param_->devID.c_str(),    "Device ID", &status);
	fits_write_key(fitsptr, TSTRING, "SITENAME", (void*)param_->siteName.c_str(), "observation site name", &status);
	fits_write_key(fitsptr, TDOUBLE, "SITELON",  (void*)&param_->siteLon,         "observation site longitude @ degrees", &status);
	fits_write_key(fitsptr, TDOUBLE, "SITELAT",  (void*)&param_->siteLat,         "observation site latitude @ degrees", &status);
	fits_write_key(fitsptr, TDOUBLE, "SITEALT",  (void*)&param_->siteAlt,         "observation site altitude @ meter", &status);
	fits_close_file(fitsptr, &status);

	if (status) {
		char txt[200];
		fits_get_errstatus(status, txt);
		_gLog.Write(LOG_FAULT, "[%s : %s], %s, %s", __FILE__, __FUNCTION__, filePath.c_str(), txt);
	}
	else {
		info_.lastobs = to_iso_extended_string(nfcam->dateobs);
		if (!focusMode_) {// 写入通知文件
			FILE* fp = fopen(pathNtfyProc_.c_str(), "a+");
			fprintf (fp, "%s  %s\n", dirRawImg_.c_str(), fmtFileName.str().c_str());
			fclose(fp);

	        fprintf(fpLog_, "%s  %s\n", dirRawImg_.c_str(), fmtFileName.str().c_str());
	        fflush(fpLog_);
		}
		else {// 启动图像处理 --> 调焦
			xmFrmPtr frame = xmFrame::Create();
			if (frame->Reset(filePath.string())) {
				queImg_.Push(frame);
				cvNewImg_.notify_one();
			}
		}
	}
	return status;
}

void CloudCamera::cloudadj() {
	const CameraInfo* nfCam = camPtr_->GetInfo();
	uint16_t* data = (uint16_t*) nfCam->data.get();
	uint32_t w = nfCam->wSensor;
	uint32_t h = nfCam->hSensor;
	uint32_t x0 = w / 2;
	uint32_t y0 = h / 2;
	uint32_t x1 = x0 + 256;
	uint32_t y1 = y0 + 256;
	uint32_t pos0;
	double sum(0);

	pos0 = (y0 - 256) * w;
	for (uint32_t y = y0 - 256; y < y1; ++y, pos0 += w) {
		for (uint32_t x = x0 - 256; x < x1; ++x) {
			sum += data[pos0 + x];
		}
	}
	sum /= 262144;
	expdur_ = int(expdur_ * 40000 / sum + 0.5);
	if (expdur_ < param_->expdurMin) expdur_ = param_->expdurMin;
	else if (expdur_ > param_->expdurMax) expdur_ = param_->expdurMax;
}

void CloudCamera::run() {
	boost::chrono::seconds toWait(param_->sampleCycle);
	const CameraInfo* nfCam = NULL;
	int cnt(0), coolGet(100);

	while (1) {
#ifdef ENABLE_CAMERA
		if (!camPtr_.unique()) {// 连接相机
			camPtr_ = boost::static_pointer_cast<CameraBase>(boost::shared_ptr<CameraQHY>(new CameraQHY));
			if (camPtr_->Connect()) {
				const CameraBase::CBSlot& slot = boost::bind(&CloudCamera::expose_process, this, _1, _2, _3);
				camPtr_->RegisterExpose(slot);
				camPtr_->CoolerOnoff(true, param_->coolerSet);
				nfCam   = camPtr_->GetInfo();
				coolGet = 100;
				expdur_ = param_->expdurMin;
				frmno_  = 1;
				cnt = 0;
				info_.state = WMC_SUCCESS;
				_gLog.Write("cloud camera connected");
			}
			else {
				info_.state = WMC_FAIL_CONNECT;
				camPtr_.reset();
				if (++cnt == 1) _gLog.Write(LOG_FAULT, "[%s:%s], failed to connect camera", __FILE__, __FUNCTION__);
			}
		}
#endif
		if (camPtr_.unique()) {
			if (nfCam->state == CAMERA_ERROR) {// 故障
				_gLog.Write(LOG_FAULT, "[%s:%s:%d], errorcode = %d", __FILE__, __FUNCTION__, __LINE__, nfCam->errcode);
				camPtr_->Disconnect();
				camPtr_.reset();
			}
			else if (nfCam->state == CAMERA_IDLE) {// 新的曝光
				// 条件1: 相机空闲
				// 条件2: 制冷稳定
				if (!camPtr_->Expose(expdur_)) {
					_gLog.Write(LOG_WARN, "[%s:%s:%d], errorcode = %d", __FILE__, __FUNCTION__, __LINE__, nfCam->errcode);
				}
				else cnt = 0;
			}
			else if (nfCam->state == CAMERA_EXPOSE && ++cnt >= 2) {// 长时间无读出
				_gLog.Write(LOG_WARN, "long time no readout");
				info_.state = WMC_FAIL_READOUT;
				camPtr_->AbortExpose();
			}
		}
		if (!focusMode_) boost::this_thread::sleep_for(toWait);
		else boost::this_thread::sleep_for(boost::chrono::milliseconds(int(expdur_ * 1000)));
	}
}

/**
 * @brief 线程: 处理图像, 统计半高全宽
 */
void CloudCamera::thread_reduce() {
	boost::mutex mtx;
	MtxLck lck(mtx);
	int step;

	invSEx_.Prepare(param_);
	while (focusMode_) {
		if (queImg_.Empty()) cvNewImg_.wait(lck);

		xmFrmPtr frame = queImg_.Pop();
		ptime now = second_clock::universal_time();
		if ((now - from_iso_extended_string(frame->dateObs)).total_seconds() > 60) {
			_gLog.Write(LOG_WARN, "[%s] was too old, procerss might be blocked",
				frame->fileName.c_str());
		}
		else if (!invSEx_.DoIt(frame) && frame->fwhm > 1.0) {
			queFwhm_.push_back(frame->fwhm);
			if (queFwhm_.size() < FOCUS_FRAME_MAX) continue;
			else if (queFwhm_.size() > FOCUS_FRAME_MAX) queFwhm_.pop_front();

			// 统计
			double sum(0.0), sq(0.0), mean, sigma;
			for (int i = 0; i < FOCUS_FRAME_MAX; ++i) {
				sum += queFwhm_[i];
				sq  += queFwhm_[i] * queFwhm_[i];
			}
			mean = sum / FOCUS_FRAME_MAX;
			sigma= (sq - mean * sum) / (FOCUS_FRAME_MAX - 1);
			sigma= sigma > 0.0 ? sqrt(sigma) : 0.0;
			_gLog.Write("%s : FWHM = %.1lf, sigma = %.2lf",
				sigma <= FOCUS_CONFIDENCE ? "--->>> GOOD <<<---" : "!!! BAD !!!",
				mean, sigma);
			if (sigma <= FOCUS_CONFIDENCE
				&& (mean - FWHM_EXPECT) > FWHM_EXPECT_ERROR
				&& focusMode_ == FOCUS_AUTO) {
				if (focusMode_ == FOCUS_AUTO) {// 自动调焦
					if (!focusAlgo_.Push(mean, step)) {
						ProtoFocusMove proto;
						proto.step = step;
						udpFocusPtr_->Write(&proto, sizeof(ProtoFocusMove));
						_gLog.Write("AutoFocou[Move]: %d", step);
					}
					else {
						ProtoFocusEnd proto;
						udpFocusPtr_->Write(&proto, sizeof(ProtoFocusEnd));
						focusMode_ = 0;
						_gLog.Write("AutoFocou stopped. the last FWHM is %4.1f", mean);
					}
				}
			}
		}
		else {
			queFwhm_.pop_front();
		}
	}
}
