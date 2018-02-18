
find_package(OptimFROG REQUIRED)

amp_add_plugin(optimfrog input.cpp)
target_link_libraries(optimfrog PRIVATE OptimFROG::OptimFROG)

