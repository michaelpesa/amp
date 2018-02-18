
cmake_minimum_required(VERSION 3.4)


function(amp_add_plugin plugin)
    add_library(${plugin} MODULE ${ARGN})

    target_link_libraries(${plugin} PRIVATE AMP::Runtime)
    set_target_properties(${plugin} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        PREFIX "amp_plugin_")

    if(APPLE)
        set_target_properties(${plugin} PROPERTIES SUFFIX ".dylib")
        target_link_libraries(${plugin} PRIVATE "-undefined dynamic_lookup")
    endif()
endfunction()


if(APPLE)
    add_subdirectory(coreaudio)
endif()

add_subdirectory(demux)
add_subdirectory(ffmpeg)
add_subdirectory(filter)
add_subdirectory(flac)
add_subdirectory(musepack)
add_subdirectory(optimfrog)
add_subdirectory(opus)
add_subdirectory(soxr)
add_subdirectory(vorbis)
add_subdirectory(wavpack)

