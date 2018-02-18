
find_path(SOXR_INCLUDE_DIR soxr.h)
find_library(SOXR_LIBRARY soxr)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SOXR
    REQUIRED_VARS SOXR_LIBRARY SOXR_INCLUDE_DIR)

mark_as_advanced(SOXR_INCLUDE_DIR SOXR_LIBRARY)

if(SOXR_FOUND AND NOT TARGET SoXR::SoXR)
    add_library(SoXR::SoXR UNKNOWN IMPORTED)
    set_target_properties(SoXR::SoXR PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${SOXR_INCLUDE_DIR}
        IMPORTED_LOCATION ${SOXR_LIBRARY})
endif()


