MESSAGE(STATUS "Searching SDK for OpenCV ...")

# FIND_LIBRARY(OPENCV_CORE_LIB NAMES opencv_core)
FIND_LIBRARY(OPENCV_IMGPROC_LIB NAMES opencv_imgproc
	PATHS "/Applications/EZCAP.app/Contents/Frameworks")
# FIND_LIBRARY(OPENCV_HIGHGUI_LIB NAMES opencv_highgui)
