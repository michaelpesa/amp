
find_path(LZ4_INCLUDE_DIR lz4.h)
find_library(LZ4_LIBRARY lz4)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LZ4
    REQUIRED_VARS LZ4_LIBRARY LZ4_INCLUDE_DIR)

mark_as_advanced(LZ4_INCLUDE_DIR LZ4_LIBRARY)

if(LZ4_FOUND AND NOT TARGET LZ4::LZ4)
    add_library(LZ4::LZ4 UNKNOWN IMPORTED)
    set_target_properties(LZ4::LZ4 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${LZ4_INCLUDE_DIR}
        IMPORTED_LOCATION ${LZ4_LIBRARY})
endif()

