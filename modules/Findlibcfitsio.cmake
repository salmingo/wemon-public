MESSAGE(STATUS "Searching SDK for CFitsIO ...")

FIND_PATH(CFITSIO_INC NAMES fitsio.h PATH_SUFFIXES cfitsio)

FIND_LIBRARY(CFITSIO_LIB NAMES cfitsio)
