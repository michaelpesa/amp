
find_package(GTest REQUIRED COMPONENTS GTest Main)

add_executable(amp_test
    ../src/core/base64.cpp
    ../src/core/cpu.cpp
    ../src/core/crc.cpp
    ../src/core/error.cpp
    ../src/core/filesystem.cpp
    ../src/core/md5.cpp
    ../src/core/numeric.cpp
    ../src/core/rbtree.cpp
    ../src/core/u8string.cpp
    ../src/core/uri.cpp
    ../src/media/cue_sheet.cpp
    ../src/media/tags.cpp
    audio_packet_test.cpp
    base64_test.cpp
    bitops_test.cpp
    cue_sheet_test.cpp
    io_buffer_test.cpp
    io_reader_test.cpp
    crc_test.cpp
    filesystem_test.cpp
    flat_map_test.cpp
    flat_set_test.cpp
    intrusive_set_test.cpp
    intrusive_slist_test.cpp
    md5_test.cpp
    media_dictionary_test.cpp
    numeric_test.cpp
    optional_test.cpp
    spsc_queue_test.cpp
    string_test.cpp
    u8string_test.cpp
    uri_test.cpp)

target_include_directories(amp_test PRIVATE
    "../src")
target_compile_definitions(amp_test PRIVATE
    AMP_DEBUG)
target_link_libraries(amp_test
    AMP::Runtime
    GTest::GTest
    GTest::Main)

add_test(amp_test amp_test)
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    DEPENDS amp_test)

