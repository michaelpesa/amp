
find_package(MPCDec REQUIRED)

amp_add_plugin(musepack input.cpp)
target_link_libraries(musepack PRIVATE MPC::Dec)

