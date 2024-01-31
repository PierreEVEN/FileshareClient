cmake_minimum_required(VERSION 3.13)

include(FetchContent)

project(FileshareClient)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    find_package(unofficial-curlpp CONFIG REQUIRED)
else()
    FetchContent_Declare(
            unofficial-curlpp
            GIT_REPOSITORY https://github.com/jpbarrette/curlpp.git
            GIT_TAG        v0.8.1
    )
    FetchContent_MakeAvailable(unofficial-curlpp)
endif()

find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.hpp)

add_executable(fileshare ${SOURCES})
target_include_directories(fileshare PRIVATE src/)
target_link_libraries(fileshare PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_link_libraries(fileshare PRIVATE unofficial::curlpp::curlpp)
else()
    target_link_libraries(fileshare PRIVATE curlpp)
endif()