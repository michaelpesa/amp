
find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED COMPONENTS Vorbis File)

amp_add_plugin(vorbis decoder.cpp input.cpp)
target_link_libraries(vorbis PRIVATE Ogg::Ogg Vorbis::Vorbis Vorbis::File)

