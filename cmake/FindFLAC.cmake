
find_path(FLAC_INCLUDE_DIR FLAC/all.h)
find_library(FLAC_LIBRARY FLAC)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(FLAC
    REQUIRED_VARS FLAC_LIBRARY FLAC_INCLUDE_DIR)

mark_as_advanced(FLAC_INCLUDE_DIR FLAC_LIBRARY)

if(FLAC_FOUND AND NOT TARGET FLAC::FLAC)
    add_library(FLAC::FLAC UNKNOWN IMPORTED)
    set_target_properties(FLAC::FLAC PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${FLAC_INCLUDE_DIR}
        IMPORTED_LOCATION ${FLAC_LIBRARY})
endif()

