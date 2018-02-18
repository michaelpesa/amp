
find_library(AudioToolbox AudioToolbox)
find_library(AudioUnit AudioUnit)
find_library(CoreAudio CoreAudio)
find_library(CoreFoundation CoreFoundation)

amp_add_plugin(coreaudio decoder.cpp output.cpp)
target_link_libraries(coreaudio PRIVATE
    ${AudioToolbox}
    ${AudioUnit}
    ${CoreAudio}
    ${CoreFoundation})

