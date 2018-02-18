
find_path(OGG_INCLUDE_DIR ogg/ogg.h)
find_library(OGG_LIBRARY ogg)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OGG
    REQUIRED_VARS OGG_LIBRARY OGG_INCLUDE_DIR)

mark_as_advanced(OGG_INCLUDE_DIR OGG_LIBRARY)

if(OGG_FOUND AND NOT TARGET Ogg::Ogg)
    add_library(Ogg::Ogg UNKNOWN IMPORTED)
    set_target_properties(Ogg::Ogg PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${OGG_INCLUDE_DIR}
        IMPORTED_LOCATION ${OGG_LIBRARY})
endif()

