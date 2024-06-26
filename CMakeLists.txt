cmake_minimum_required (VERSION 3.16)

# Build qpl.
set(CMAKE_BUILD_TYPE Release)
add_subdirectory(qpl)

# gcc setup
add_definitions(-std=c++17 -O3 -march=native)
add_definitions(-Wall -Wextra -Wsign-conversion -Wformat -Wformat-security)
add_definitions(-pedantic)
add_definitions(-fstack-protector -fPIE -fPIC)
add_definitions(-D_FORTIFY_SOURCE=2)

#
add_compile_definitions(GLOG_USE_GLOG_EXPORT)

# Stuff.
find_package (glog REQUIRED)
find_package (gflags REQUIRED)

#
add_executable(iaa_bench src/main.cc)

target_link_libraries(iaa_bench PUBLIC qpl benchmark glog gflags)
