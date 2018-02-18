
find_package(Ogg REQUIRED)
find_package(FLAC REQUIRED)

amp_add_plugin(flac input.cpp)
target_link_libraries(flac PRIVATE Ogg::Ogg FLAC::FLAC)

