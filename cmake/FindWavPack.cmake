
find_path(WAVPACK_INCLUDE_DIR wavpack/wavpack.h)
find_library(WAVPACK_LIBRARY wavpack)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WAVPACK
    REQUIRED_VARS WAVPACK_LIBRARY WAVPACK_INCLUDE_DIR)

mark_as_advanced(WAVPACK_INCLUDE_DIR WAVPACK_LIBRARY)

if(WAVPACK_FOUND AND NOT TARGET WavPack::WavPack)
    add_library(WavPack::WavPack UNKNOWN IMPORTED)
    set_target_properties(WavPack::WavPack PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${WAVPACK_INCLUDE_DIR}
        IMPORTED_LOCATION ${WAVPACK_LIBRARY})
endif()


