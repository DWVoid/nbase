cmake_minimum_required(VERSION 3.17)
project(NMSG)
# set required standard to 20
set(CMAKE_CXX_STANDARD 20)
# fine boost
find_package(Boost COMPONENTS system fiber)
# locate source files
file(GLOB_RECURSE SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*)

add_executable(NMSG ${SRC})
target_link_libraries(NMSG PRIVATE ${Boost_LIBRARIES})
target_include_directories(NMSG PRIVATE ${Boost_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# set additional compiler flags
if (MSVC)
    target_compile_options(NMSG PRIVATE /await)
endif()
