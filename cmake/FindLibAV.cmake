
include(FindPackageHandleStandardArgs)


find_path(LIBAVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(LIBAVUTIL_LIBRARY avutil)
find_package_handle_standard_args(LIBAVUTIL
    REQUIRED_VARS LIBAVUTIL_LIBRARY LIBAVUTIL_INCLUDE_DIR)
mark_as_advanced(LIBAVUTIL_INCLUDE_DIR LIBAVUTIL_LIBRARY)


find_path(LIBAVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(LIBAVCODEC_LIBRARY avcodec)
find_package_handle_standard_args(LIBAVCODEC
    REQUIRED_VARS LIBAVCODEC_LIBRARY LIBAVCODEC_INCLUDE_DIR)
mark_as_advanced(LIBAVCODEC_INCLUDE_DIR LIBAVCODEC_LIBRARY)


if(LIBAVUTIL_FOUND AND NOT TARGET LibAV::Util)
    add_library(LibAV::Util UNKNOWN IMPORTED)
    set_target_properties(LibAV::Util PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${LIBAVUTIL_INCLUDE_DIR}
        IMPORTED_LOCATION ${LIBAVUTIL_LIBRARY})
endif()

if(LIBAVCODEC_FOUND AND NOT TARGET LibAV::Codec)
    add_library(LibAV::Codec UNKNOWN IMPORTED)
    set_target_properties(LibAV::Codec PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${LIBAVCODEC_INCLUDE_DIR}
        IMPORTED_LOCATION ${LIBAVCODEC_LIBRARY})
endif()
