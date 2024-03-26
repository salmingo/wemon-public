MESSAGE(STATUS "Searching SDK for QHYCMOS ...")

# FIND_PATH(QHY_INC NAMES qhyccd.h)
FIND_LIBRARY(QHY_LIB NAMES qhyccd
	PATHS "/Applications/EZCAP.app/Contents/Frameworks")
