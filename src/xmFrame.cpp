
#include <fitsio.h>
#include <longnam.h>
#include <boost/filesystem.hpp>
#include "xmFrame.h"
#include "GLog.h"

using namespace boost::filesystem;

bool  xmFrame::Reset(const string& pathImageFile) {
	// 重置关键成员变量
	astroFix = photoFix = false;
	fwhm = fwhmErr = 0.0;
	stars.clear();
	// 解析文件路径
	path pathName(pathImageFile);
	fileName = pathName.filename().string();
	dirName  = pathName.parent_path().string();
	filePath = pathName.string();

	// 读取FITS头
	int status(0);
	int naxis = 0;
	long naxes[2];
	fitsfile* hfits;

	fits_open_file(&hfits, pathImageFile.c_str(), READONLY, &status);
	fits_get_img_dim(hfits, &naxis, &status);
	if (naxis != 2) return false;

	fits_get_img_size(hfits, naxis, naxes, &status);
	width  = naxes[0];
	height = naxes[1];

	if (!status) {// 读取关键字
		char dateobs[40], timeobs[40];
		fits_read_key(hfits, TDOUBLE, "EXPTIME", &expTime, 0, &status);
		fits_read_key(hfits, TSTRING, "DATE-OBS", dateobs, 0, &status);
		if (!strstr(dateobs, "T")) {
			fits_read_key(hfits, TSTRING, "TIME-OBS", timeobs, 0, &status);
			strcat(dateobs, "T");
			strcat(dateobs, timeobs);
		}
		dateObs = dateobs;
	}
	fits_close_file(hfits, &status);

	if (status) {
		char errtxt[100];
		fits_get_errstatus(status, errtxt);
		_gLog.Write(LOG_FAULT, "[%s]: %s", pathImageFile.c_str(), errtxt);
	}

	return status == 0;
}
