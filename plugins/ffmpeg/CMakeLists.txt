
find_package(LibAV REQUIRED COMPONENTS Util Codec)

amp_add_plugin(ffmpeg decoder.cpp)
target_link_libraries(ffmpeg PRIVATE LibAV::Util LibAV::Codec)

