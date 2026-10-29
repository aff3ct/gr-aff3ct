find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_AFF3CT gnuradio-aff3ct)

FIND_PATH(
    GR_AFF3CT_INCLUDE_DIRS
    NAMES gnuradio/aff3ct/api.h
    HINTS $ENV{AFF3CT_DIR}/include
        ${PC_AFF3CT_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_AFF3CT_LIBRARIES
    NAMES gnuradio-aff3ct
    HINTS $ENV{AFF3CT_DIR}/lib
        ${PC_AFF3CT_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-aff3ctTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_AFF3CT DEFAULT_MSG GR_AFF3CT_LIBRARIES GR_AFF3CT_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_AFF3CT_LIBRARIES GR_AFF3CT_INCLUDE_DIRS)
