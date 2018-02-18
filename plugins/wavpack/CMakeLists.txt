
find_package(WavPack REQUIRED)

amp_add_plugin(wavpack input.cpp)
target_link_libraries(wavpack PRIVATE WavPack::WavPack)

