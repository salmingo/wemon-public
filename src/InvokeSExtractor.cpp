
#include <boost/filesystem.hpp>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "InvokeSExtractor.h"
#include "GLog.h"
#include "xmImageDef.h"

#define TEMP_DIR	"/tmp"
#define TEMP_SEX	"/tmp/default.sex"
#define TEMP_PARAM	"/tmp/default.param"
#define TEMP_CONV	"/tmp/default.conv"
#define TEMP_NNW	"/tmp/default.nnw"

using namespace boost::filesystem;

InvokeSExtractor::InvokeSExtractor()
	: param_(NULL)
	, prepared_(false)
	, running_(false)
	, starCount_(0) {
	headLink_ = new xmStarLink;
	headLink_->prev = headLink_;
	headLink_->next = headLink_;
}

InvokeSExtractor::~InvokeSExtractor() {
	free_star_link(headLink_);
	delete headLink_;
}

/**
 * @brief 设置配置参数
 * @param param 配置参数
 */
bool InvokeSExtractor::Prepare(const Parameter* param) {
	if (!param) return false;
	param_ = param;
	return (prepared_ = scan_executor() && generate_configuration());
}

/**
 * @brief 处理图像, 提取图像内星像及其特征
 * @param frame  图像帧指针
 * @return 图像处理结果
 */
int InvokeSExtractor::DoIt(xmFrmPtr frame) {
	if (!prepared_) return 1;

	path pathCat(frame->filePath);
	pathCat.replace_extension("cat");

	pid_t pid0 = fork();
	if (pid0 > 0) {// 主进程
		pid_t pid;
		int status, rslt(0);

		frame_   = frame;
		running_ = true;
		while ((pid = waitpid(pid0, &status, WNOHANG | WUNTRACED)) != pid0 && pid != -1);

		if (pid == pid0) {
			if (resolve_catalog(pathCat.c_str()) < STAR_COUNT_MIN) {
				_gLog.Write(LOG_WARN, "%s, no enough stars found", frame->fileName.c_str());
				rslt = 4;
			}
			else if (!stat_quality()) {
                _gLog.Write(LOG_WARN, "%s, bad image quality", frame->fileName.c_str());
				rslt = 5;
			}
			else {
				// if (!(frame->bTrailing = stat_incline())) {// 是否大部分星像拖尾? == false
				// 	stat_fwhm();
				// }
				// else if (stat_elong()) {// 剔除无效星像
				// 	// remove_polluted();
				// }
				stat_fwhm();
				link2frame();
				if (frame->fwhm > 1.0) {
					_gLog.Write("%s, star count = %6u, fwhm = %4.1f, sigma = %5.2f",
						frame->fileName.c_str(), frame->stars.size(),
						frame->fwhm, frame->fwhmErr);
				}
				else {
					_gLog.Write("%s, star count = %6u", frame->fileName.c_str(), frame->stars.size());
				}
			}
			free_star_link(headLink_);
		}
		else {// if (pid == -1)
			_gLog.Write(LOG_FAULT, "[%s:%d], %s, errno = %d", __FILE__, __LINE__, frame->fileName.c_str(), errno);
			rslt = 3;
		}

		frame_.reset();
		running_ = false;
		return rslt;
	}
	else if (pid0 < 0) {
		return 2;
	}

	close(2);
	current_path(path(TEMP_DIR));
	execl(pathExe_.c_str(), nameExe_.c_str(), frame->filePath.c_str(), "-CATALOG_NAME", pathCat.c_str(), 0);
	exit(0);
}

bool InvokeSExtractor::scan_executor() {
	const int N = 6;
	const string exeList[] = {
		"/usr/bin/sex",
		"/usr/local/bin/sex",
		"/opt/homebrew/bin/sex",
		"/usr/bin/source-extractor",
		"/usr/local/bin/source-extractor",
		"/opt/homebrew/bin/source-extractor"
	};

	for (int i = 0; i < N; ++i) {
		if (exists(exeList[i])) {
			pathExe_ = exeList[i];
			nameExe_ = i < 3 ? "sex" : "source-extractor";
			return true;
		}
	}
	return false;
}

bool InvokeSExtractor::generate_configuration() {
	return (generate_default_sex()
		&& generate_default_param()
		&& generate_default_conv()
		&& generate_default_nnw());
}

bool InvokeSExtractor::generate_default_sex() {
	if (!exists(TEMP_SEX)) {
		FILE* fp = fopen(TEMP_SEX, "w");
		fprintf (fp,
			"# Default configuration file for SExtractor 2.25.0\n"
			"#-------------------------------- Catalog ------------------------------------\n"
			"CATALOG_NAME     test.cat\n"
			"CATALOG_TYPE     ASCII_HEAD\n"
			"PARAMETERS_NAME  default.param\n\n"
			"#------------------------------- Extraction ----------------------------------\n"
			"DETECT_TYPE      CCD\n"
			"DETECT_MINAREA   %d\n"
			"DETECT_THRESH    1.5\n"
			"ANALYSIS_THRESH  %.1f\n"
			"FILTER           Y\n"
			"FILTER_NAME      default.conv\n"
			// "DEBLEND_NTHRESH  32\n"
			// "DEBLEND_MINCONT  0.005\n"
			"DEBLEND_NTHRESH  4\n"
			"DEBLEND_MINCONT  1\n"
			"CLEAN            Y\n"
			// "CLEAN_PARAM      1.0\n"
			"CLEAN_PARAM      2\n"
			"WEIGHT_TYPE      NONE\n\n"
			"#------------------------------ Photometry -----------------------------------\n"
			"PHOT_APERTURES   5\n"
			"PHOT_AUTOPARAMS  2.5, 3.5\n"
			"PHOT_PETROPARAMS 2.0, 3.5\n"
	        "PHOT_AUTOAPERS   5.0,10.0\n"
			"SATUR_LEVEL      50000.0\n"
			"SATUR_KEY        SATURATE\n"
			"MAG_ZEROPOINT    22.0\n"
			"MAG_GAMMA        4.0\n"
			"GAIN             1.0\n"
			"GAIN_KEY         GAIN\n"
			"PIXEL_SCALE      1.0\n\n"
			"#------------------------- Star/Galaxy Separation ----------------------------\n"
			"SEEING_FWHM      3\n"
			"STARNNW_NAME     default.nnw\n\n"
			"#------------------------------ Background -----------------------------------\n"
			"BACK_TYPE        AUTO\n"
			"BACK_VALUE       0.0\n"
			"BACK_SIZE        64\n"
			"BACK_FILTERSIZE  3\n"
			"BACKPHOTO_TYPE   LOCAL\n"
			"BACKPHOTO_THICK  24\n\n"
			"#------------------------------ Check Image ----------------------------------\n"
			"CHECKIMAGE_TYPE  NONE\n"
			"CHECKIMAGE_NAME  check.fits\n\n"
			"#--------------------- Memory (change with caution!) -------------------------\n"
			"MEMORY_OBJSTACK  3000\n"
			"MEMORY_PIXSTACK  300000\n"
			"MEMORY_BUFSIZE   1024\n\n"
			"#----------------------------- Miscellaneous ---------------------------------\n"
			"VERBOSE_TYPE     QUIET\n"
			"HEADER_SUFFIX    .head\n"
			"WRITE_XML        N\n"
			"XML_NAME         sex.xml\n",
			3, 5.
		);
		fclose(fp);
	}
	return exists(TEMP_SEX);
}

bool InvokeSExtractor::generate_default_param() {
	if (!exists(TEMP_PARAM)) {
		FILE* fp = fopen(TEMP_PARAM, "w");
		fprintf (fp,
			"X_IMAGE\n"
			"Y_IMAGE\n"
			"ELONGATION\n"
			"ISOAREA_IMAGE\n"
			"FWHM_IMAGE\n"
			"THETA_IMAGE\n"
			"FLUX_BEST\n"
	        "FLUXERR_BEST\n"
			"FLUX_MAX\n"
			"MAG_BEST\n"
			"MAGERR_BEST\n"
		);
	    fclose(fp);
	}
	return exists(TEMP_PARAM);
}

bool InvokeSExtractor::generate_default_conv() {
	if (!exists(TEMP_CONV)) {
		int size =5;
		FILE* fp = fopen(TEMP_CONV, "w");
		fprintf (fp, "CONV NORM\n");
		// 高斯滤波
		double fwhm = 3.0;	// 设置星像FWHM=3.0. 理想星像一般调焦至2-3, 设置为3, 对星像做膨胀处理
		double sigma = fwhm / 2.0 / sqrt(2.0 * log(2.0));
		double ratio = -0.5 / (sigma * sigma); // = -1 / (2 * sigma * sigma)
		double *kernel = new double[size * size];
		double *ptr = kernel;
		double sum(0.0);
		double dy2;
		int center = size / 2;
		for (int j = 0; j < size; ++j) {
			dy2 = (j - center) * (j - center);
			for (int i = 0; i < size; ++i, ++ptr) {
				*ptr = exp(((i - center) * (i - center) + dy2) * ratio);
				sum += *ptr;
			}
		}
		ptr = kernel;
		for (int j = 0; j < size; ++j) {
			for (int i = 0; i < size; ++i, ++ptr) {
				*ptr = *ptr / sum;
				fprintf (fp, "%8.6lf  ", *ptr);
			}
			fprintf (fp, "\n");
		}

		delete []kernel;

		fclose(fp);
	}
	return exists(TEMP_CONV);
}

bool InvokeSExtractor::generate_default_nnw() {
	if (!exists(TEMP_NNW)) {
		FILE* fp = fopen(TEMP_NNW, "w");
		fprintf (fp,
			"NNW\n"
			"# Neural Network Weights for the SExtractor star/galaxy classifier (V1.3)\n"
			"# inputs:	9 for profile parameters + 1 for seeing.\n"
			"# outputs:	``Stellarity index'' (0.0 to 1.0)\n"
			"# Seeing FWHM range: from 0.025 to 5.5'' (images must have 1.5 < FWHM < 5 pixels)\n"
			"# Optimized for Moffat profiles with 2<= beta <= 4.\n"
			"\n"
			" 3 10 10  1\n"
			"\n"
			"-1.56604e+00 -2.48265e+00 -1.44564e+00 -1.24675e+00 -9.44913e-01 -5.22453e-01  4.61342e-02  8.31957e-01  2.15505e+00  2.64769e-01\n"
			" 3.03477e+00  2.69561e+00  3.16188e+00  3.34497e+00  3.51885e+00  3.65570e+00  3.74856e+00  3.84541e+00  4.22811e+00  3.27734e+00\n"
			"\n"
			"-3.22480e-01 -2.12804e+00  6.50750e-01 -1.11242e+00 -1.40683e+00 -1.55944e+00 -1.84558e+00 -1.18946e-01  5.52395e-01 -4.36564e-01 -5.30052e+00\n"
			" 4.62594e-01 -3.29127e+00  1.10950e+00 -6.01857e-01  1.29492e-01  1.42290e+00  2.90741e+00  2.44058e+00 -9.19118e-01  8.42851e-01 -4.69824e+00\n"
			"-2.57424e+00  8.96469e-01  8.34775e-01  2.18845e+00  2.46526e+00  8.60878e-02 -6.88080e-01 -1.33623e-02  9.30403e-02  1.64942e+00 -1.01231e+00\n"
			" 4.81041e+00  1.53747e+00 -1.12216e+00 -3.16008e+00 -1.67404e+00 -1.75767e+00 -1.29310e+00  5.59549e-01  8.08468e-01 -1.01592e-02 -7.54052e+00\n"
			" 1.01933e+01 -2.09484e+01 -1.07426e+00  9.87912e-01  6.05210e-01 -6.04535e-02 -5.87826e-01 -7.94117e-01 -4.89190e-01 -8.12710e-02 -2.07067e+01\n"
			"-5.31793e+00  7.94240e+00 -4.64165e+00 -4.37436e+00 -1.55417e+00  7.54368e-01  1.09608e+00  1.45967e+00  1.62946e+00 -1.01301e+00  1.13514e-01\n"
			" 2.20336e-01  1.70056e+00 -5.20105e-01 -4.28330e-01  1.57258e-03 -3.36502e-01 -8.18568e-02 -7.16163e+00  8.23195e+00 -1.71561e-02 -1.13749e+01\n"
			" 3.75075e+00  7.25399e+00 -1.75325e+00 -2.68814e+00 -3.71128e+00 -4.62933e+00 -2.13747e+00 -1.89186e-01  1.29122e+00 -7.49380e-01  6.71712e-01\n"
			"-8.41923e-01  4.64997e+00  5.65808e-01 -3.08277e-01 -1.01687e+00  1.73127e-01 -8.92130e-01  1.89044e+00 -2.75543e-01 -7.72828e-01  5.36745e-01\n"
			"-3.65598e+00  7.56997e+00 -3.76373e+00 -1.74542e+00 -1.37540e-01 -5.55400e-01 -1.59195e-01  1.27910e-01  1.91906e+00  1.42119e+00 -4.35502e+00\n"
			"\n"
			"-1.70059e+00 -3.65695e+00  1.22367e+00 -5.74367e-01 -3.29571e+00  2.46316e+00  5.22353e+00  2.42038e+00  1.22919e+00 -9.22250e-01 -2.32028e+00\n"
			"\n"
			" 0.00000e+00\n"
			" 1.00000e+00\n"
		);
		fclose(fp);
	}
	return exists(TEMP_NNW);
}

int InvokeSExtractor::resolve_catalog(const char* pathCat) {
	free_star_link(headLink_);	// 健壮性, 必要
	starCount_ = 0;
	xmStarLink *oldLink = headLink_;
	xmStarLink *newLink;
	double snr0 = 3;
	// 读取并解析文件
	FILE* fpCat = fopen(pathCat, "r");
	static char line[LINE_MAX];

	// X Y A B AREA FWHM THETA FLUX FLUXERR FLUX_MAX MAG MAGERR
	while (fgets(line, LINE_MAX, fpCat)) {
		if (line[0] == '#') continue;

		xmStarPtr star = xmStar::Create();
		sscanf(line, "%lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf",
			&star->x, &star->y,
			&star->elong,
			&star->area, &star->fwhm, &star->theta,
			&star->flux, &star->fluxErr, &star->fluxMax,
			&star->mag, &star->magErr);

		// 判据
		// - 流量 >= 1
		// - 面积 >= STAR_AREA_MIN, 剔除热点
		// - 信噪比 >= snr0
		// - FWHM > 1. 剔除严重不符合轮廓分布
		if (star->flux > 1.
				&& star->area >= STAR_AREA_MIN
				&& (star->snr = star->flux / star->fluxErr) >= snr0
				&& star->fwhm > 1.) {
			newLink = new xmStarLink(star);
			insert_star_link(oldLink, newLink);
			oldLink = newLink;

			++starCount_;
		}
	}

	fclose(fpCat);

#ifndef NDEBUG
	try {
		remove(path(pathCat));
	}
	catch(filesystem_error& ex) {
		_gLog.Write(LOG_WARN, "[%s:%d], %s", __FILE__, __LINE__, ex.what());
	}
#endif

	return starCount_;
}

bool InvokeSExtractor::stat_quality() {
	return true;
}

bool InvokeSExtractor::stat_incline() {
	int N(0);
	int loopcnt(0), loopmax(10);
	double snr0(5);
	double sum(0.), sq(0.), low(0.), high(0.);
	double mean, sig0, sig1;
	double vmin(1E30), vmax(-1E30), val;
	// 统计倾角
	xmStarLink* now = headLink_->next;
	while (now != headLink_) {
		if (now->star->snr > snr0) {
			if ((val = now->star->theta) < -80.) {
				val += 180.;
				now->star->theta = val;
			}
			if (val < vmin) vmin = val;
			if (val > vmax) vmax = val;
			sum += val;
			sq  += val * val;
			++N;
		}

		now = now->next;
	}
	mean = sum / N;
	sig0 = (sq - mean * sum) / (N - 1);
	sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
	low  = mean - 2. * sig0;
	high = mean + 2. * sig0;

	if (low < vmin && high > vmax) {
		// 无统计倾角. 大部分星成点像
		return false;
	}
#ifdef NDEBUG
	printf ("theta, use %d of %d:\n"
		"(min, max) = %5.1f, %5.1f\n"
		"(mean, sig)= %5.1f, %5.1f\n",
		N, starCount_,
		vmin, vmax, mean, sig0);
#endif

	// 若有统计特性, 则迭代找到峰值数值
	do {
		N = 0;
		sig1 = sig0;
		sum = sq = 0.;
		now = headLink_->next;
		while (now != headLink_) {
			if (now->star->snr > snr0 && (val = now->star->theta) >= low && val <= high) {
				sum += val;
				sq  += val * val;
				++N;
			}
			now = now->next;
		}
		mean = sum / N;
		sig0 = (sq - mean * sum) / (N - 1);
		sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
		low = mean - 2. * sig0;
		high= mean + 2. * sig0;

#ifdef NDEBUG
		printf ("theta, loop = %d, use %d of %d:\n"
			"(mean, sig)= %5.1f, %5.1f\n",
			loopcnt + 1, N, starCount_,
			mean, sig0);
#endif
	} while (++loopcnt < loopmax && sig1 / sig0 > 1.1);
	frame_->incl    = mean;
	frame_->inclErr = sig0;

	return true;
}

void InvokeSExtractor::stat_fwhm() {
	int N(0);
	double snr0(5);
	double sum(0.), sq(0.), mean(0.), sig0(0.), sig1(0.);
	double low(0.), high(0.);
	double vmin(1E30), vmax(-1E30), val;
	double x0(frame_->width * 0.5 + 0.5);
	double y0(frame_->height * 0.5 + 0.5);
	double wHalf = 0.3 * frame_->width;
	double hHalf = 0.3 * frame_->height;
	// 统计半高全宽
	xmStarPtr star;
	xmStarLink* now = headLink_->next;
	while (now != headLink_) {
		star = now->star;
		if ((star->inStat = star->snr > snr0 && fabs(star->x - x0) <= wHalf && fabs(star->y - y0) <= hHalf)) {
			if ((val = now->star->fwhm) < vmin) vmin = val;
			if (val > vmax) vmax = val;
			sum += val;
			sq  += val * val;
			++N;
		}

		now = now->next;
	}
	mean = sum / N;
	sig0 = (sq - mean * sum) / (N - 1);
	sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
	low = mean - 2. * sig0;
	high= mean + 2. * sig0;

	// FWHM分布严重偏离正态分布. 原则上不能用于调焦
	if (low < vmin && high > vmax) return ;
#ifdef NDEBUG
	printf ("FWHM, use %d of %d:\n"
		"(min, max) = %5.1f, %5.1f\n"
		"(mean, sig)= %5.1f, %5.1f\n",
		N, starCount_,
		vmin, vmax, mean, sig0);
#endif

	// 若具有统计特性, 则迭代找到峰值位置
	do {
		N = 0;
		sig1 = sig0;
		sum = sq = 0.;
		now = headLink_->next;

		while (now != headLink_) {
			star = now->star;
			if (star->inStat && (val = star->fwhm) >= low && val <= high) {
				sum += val;
				sq  += val * val;
				++N;
			}
			now = now->next;
		}
		mean = sum / N;
		sig0 = (sq - mean * sum) / (N - 1);
		sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
		low = mean - 2. * sig0;
		high= mean + 2. * sig0;
#ifdef NDEBUG
		printf ("FWHM, use %d of %d:\n(mean, sig)= %5.1f, %5.1f\n",
			N, starCount_, mean, sig0);
#endif
	} while (N >= 100 && sig1 / sig0 > 1.1);

	if (mean > 1.0 && mean / sig0 >= 3.0) {
		frame_->fwhm    = mean;
		frame_->fwhmErr = sig0;
	}
}

bool InvokeSExtractor::stat_elong() {
	double inclLow = frame_->incl - 5. * frame_->inclErr;	// 倾角阈值
	double inclHigh= frame_->incl + 5. * frame_->inclErr;
	double sum(0.), sq(0.), mean(0.), sig0(0.), sig1(0.);
	double low(0.), high(0.);
	double vmin(1E30), vmax(-1E30), val;
	int N(0);
	int loopcnt(0), loopmax(10);
	// 统计分布
	xmStarPtr star;
	xmStarLink* now = headLink_->next;
	while (now != headLink_) {
		star = now->star;
		if ((star->inStat = star->theta >= inclLow && star->theta <= inclHigh)) {
			if ((val = star->elong) < vmin) vmin = val;
			if (val > vmax) vmax = val;
			sum += val;
			sq  += val * val;
			++N;
		}

		now = now->next;
	}
	mean = sum / N;
	sig0 = (sq - mean * sum) / (N - 1);
	sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
	low = mean - 2. * sig0;
	high= mean + 2. * sig0;

	if (low < vmin && high > vmax) {
		// 不符合统计分布-->被污染信号过多, 无法剔除信号
		return false;
	}
#ifdef NDEBUG
	printf ("ELongation, use %d of %d:\n"
		"(min, max) = %5.1f, %5.1f\n"
		"(mean, sig)= %5.1f, %5.1f\n",
		N, starCount_,
		vmin, vmax, mean, sig0);
#endif

	// 若具有统计特性, 则迭代找到峰值位置
	do {
		N = 0;
		sig1 = sig0;
		sum = sq = 0.;
		now = headLink_->next;

		while (now != headLink_) {
			star = now->star;
			if (star->inStat && (val = now->star->elong) >= low && val <= high) {
				sum += val;
				sq  += val * val;
				++N;
			}
			now = now->next;
		}
		mean = sum / N;
		sig0 = (sq - mean * sum) / (N - 1);
		sig0 = sig0 > 0.0 ? sqrt(sig0) : 0.0;
		low = mean - 2. * sig0;
		high= mean + 2. * sig0;
#ifdef NDEBUG
		printf ("ELongation, loop = %d, use %d of %d:\n"
			"(mean, sig)= %5.1f, %5.1f\n",
			loopcnt + 1, N, starCount_,
			mean, sig0);
#endif
	} while (++loopcnt < loopmax && sig1 / sig0 > 1.1);
	elong_    = mean;
	elongErr_ = sig0;

	return true;
}

void InvokeSExtractor::remove_polluted() {
	double low = elong_ - 2.5 * elongErr_;
	double high= elong_ + 2.5 * elongErr_;

	int n(0);
	xmStarPtr star;
	xmStarLink* temp;
	xmStarLink* now = headLink_->next;
	while (now != headLink_) {
		star = now->star;
		if (star->inStat && (star->elong < low || star->elong > high)) {// 删除
			temp = now->next;
			remove_star_link(&now);
			now = temp;
			++n;
		}
		else now = now->next;
	}
	printf ("remove %d polluted stars\n", n);
}

void InvokeSExtractor::link2frame() {
	xmStarLink *now = headLink_->next;
	xmStarPtrVec &stars = frame_->stars;

	while (now != headLink_) {
		stars.push_back(now->star);
		now = now->next;
	}

#ifdef NDEBUG
	xmStarPtr star;
	path pathName(frame_->filePath);
	pathName.replace_extension("txt");
	FILE* fp = fopen(pathName.c_str(), "w");

	now = headLink_->next;
	while (now != headLink_) {
		star = now->star;
		now  = now->next;
		fprintf (fp, "%8.3lf %8.3lf %5.2lf %6.2lf %5.3lf\n",
			star->x, star->y, star->fwhm, star->mag, star->magErr);
	}
	fclose(fp);
#endif
}
