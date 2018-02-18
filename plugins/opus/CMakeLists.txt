
find_package(Ogg REQUIRED)
find_package(Opus REQUIRED COMPONENTS Opus File)

amp_add_plugin(opus decoder.cpp input.cpp)
target_link_libraries(opus PRIVATE Ogg::Ogg Opus::Opus Opus::File)

