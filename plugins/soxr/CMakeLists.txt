
find_package(SoXR REQUIRED)

amp_add_plugin(soxr resampler.cpp)
target_link_libraries(soxr PRIVATE SoXR::SoXR)

