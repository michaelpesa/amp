
include(FindPackageHandleStandardArgs)

find_path(VORBIS_INCLUDE_DIR vorbis/codec.h)
find_library(VORBIS_LIBRARY vorbis)
find_package_handle_standard_args(VORBIS
    REQUIRED_VARS VORBIS_LIBRARY VORBIS_INCLUDE_DIR)
mark_as_advanced(VORBIS_INCLUDE_DIR VORBIS_LIBRARY)

find_path(VORBISFILE_INCLUDE_DIR vorbis/vorbisfile.h)
find_library(VORBISFILE_LIBRARY vorbisfile)
find_package_handle_standard_args(VORBISFILE
    REQUIRED_VARS VORBISFILE_LIBRARY VORBISFILE_INCLUDE_DIR)
mark_as_advanced(VORBISFILE_INCLUDE_DIR VORBISFILE_LIBRARY)


if(VORBIS_FOUND AND NOT TARGET Vorbis::Vorbis)
    add_library(Vorbis::Vorbis UNKNOWN IMPORTED)
    set_target_properties(Vorbis::Vorbis PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${VORBIS_INCLUDE_DIR}
        IMPORTED_LOCATION ${VORBIS_LIBRARY})
endif()

if(VORBISFILE_FOUND AND NOT TARGET Vorbis::File)
    add_library(Vorbis::File UNKNOWN IMPORTED)
    set_target_properties(Vorbis::File PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${VORBISFILE_INCLUDE_DIR}
        IMPORTED_LOCATION ${VORBISFILE_LIBRARY})
endif()

