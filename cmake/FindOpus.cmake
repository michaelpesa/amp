
include(FindPackageHandleStandardArgs)

find_path(OPUS_INCLUDE_DIR opus/opus.h)
find_library(OPUS_LIBRARY opus)
find_package_handle_standard_args(OPUS
    REQUIRED_VARS OPUS_LIBRARY OPUS_INCLUDE_DIR)
mark_as_advanced(OPUS_INCLUDE_DIR OPUS_LIBRARY)

find_path(OPUSFILE_INCLUDE_DIR opus/opusfile.h)
find_library(OPUSFILE_LIBRARY opusfile)
find_package_handle_standard_args(OPUSFILE
    REQUIRED_VARS OPUSFILE_LIBRARY OPUSFILE_INCLUDE_DIR)
mark_as_advanced(OPUSFILE_INCLUDE_DIR OPUSFILE_LIBRARY)


if(OPUS_FOUND AND NOT TARGET Opus::Opus)
    add_library(Opus::Opus UNKNOWN IMPORTED)
    set_target_properties(Opus::Opus PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${OPUS_INCLUDE_DIR}
        IMPORTED_LOCATION ${OPUS_LIBRARY})
endif()

if(OPUSFILE_FOUND)
    set(OPUSFILE_INCLUDE_DIRS
        ${OPUSFILE_INCLUDE_DIR}
        ${OPUS_INCLUDE_DIR}/opus)

    if(NOT TARGET Opus::File)
        add_library(Opus::File UNKNOWN IMPORTED)
        set_property(TARGET Opus::File PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${OPUSFILE_INCLUDE_DIRS})
        set_property(TARGET Opus::File PROPERTY
            IMPORTED_LOCATION ${OPUSFILE_LIBRARY})
    endif()
endif()

