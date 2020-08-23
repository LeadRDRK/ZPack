# add executable with name zpack-cli to avoid conflict with the library
add_executable(zpack-cli zpack-cli.cpp)
set_property(TARGET zpack-cli PROPERTY CXX_STANDARD 11)

find_package(Boost REQUIRED COMPONENTS filesystem)
target_link_libraries(zpack-cli zpack Boost::filesystem)
# include dirs
target_include_directories(zpack-cli PRIVATE "../lib")
# set output name to zpack
set_target_properties(zpack-cli PROPERTIES OUTPUT_NAME zpack)