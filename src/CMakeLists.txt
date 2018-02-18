
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(LZ4 REQUIRED)
find_package(CURL REQUIRED)


add_executable(amp
    audio/channel_mapper.cpp
    audio/circular_buffer.cpp
    audio/filter_chain.cpp
    audio/format.cpp
    audio/pcm.cpp
    audio/player.cpp
    audio/replaygain.cpp
    core/base64.cpp
    core/cpu.cpp
    core/crc.cpp
    core/curl_stream.cpp
    core/error.cpp
    core/file_stream.cpp
    core/filesystem.cpp
    core/md5.cpp
    core/muldiv.cpp
    core/rbtree.cpp
    core/registry.cpp
    core/u8string.cpp
    core/uri.cpp
    media/ape.cpp
    media/cue_sheet.cpp
    media/id3v1.cpp
    media/id3v2.cpp
    media/playlist.cpp
    media/tags.cpp
    media/title_format.cpp
    ui/album_art_view.cpp
    ui/audio_info_bar.cpp
    ui/audio_seek_bar.cpp
    ui/audio_tool_bar.cpp
    ui/icons.qrc
    ui/main_menu_bar.cpp
    ui/main_window.cpp
    ui/media_library.cpp
    ui/media_scanner.cpp
    ui/image_server.cpp
    ui/playlist_model.cpp
    ui/playlist_view.cpp
    ui/preferences_dialog.cpp
    ui/track_info_dialog.cpp
    ui/volume_button.cpp
    main.cpp)

target_compile_definitions(amp PRIVATE
    AMP_CORE_BUILD)

target_link_libraries(amp PRIVATE
    AMP::Runtime
    Qt5::Widgets
    LZ4::LZ4
    CURL)

set_target_properties(amp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(amp PRIVATE
        "-Wno-undefined-reinterpret-cast")
endif()

if(APPLE)
    target_compile_options(amp PRIVATE
        "-fobjc-arc")

    target_link_libraries(amp PRIVATE
        "-framework AppKit"
        "-framework Foundation"
        "-Wl,-export_dynamic")

    target_sources(amp PRIVATE
        core/config_macos.mm
        ui/playlist_view_macos.mm)

    set_target_properties(amp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "amp"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.amp.amp"
        MACOSX_BUNDLE_COPYRIGHT "© 2018 Michael Pesa. All rights reserved.")
endif()

