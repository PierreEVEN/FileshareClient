cmake_minimum_required(VERSION 3.13)

project(FileshareClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-curlpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.hpp)

add_executable(FileshareClient ${SOURCES})
target_include_directories(FileshareClient PRIVATE src/)
target_link_libraries(FileshareClient PRIVATE unofficial::curlpp::curlpp)
target_link_libraries(FileshareClient PRIVATE nlohmann_json::nlohmann_json)