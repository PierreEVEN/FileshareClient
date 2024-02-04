cmake_minimum_required(VERSION 3.13)

include(FetchContent)

project(FileshareClient)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package (Python COMPONENTS Interpreter Development)
execute_process(COMMAND ${Python_EXECUTABLE} src/mime-setup.py)

 find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.hpp)

add_executable(fileshare ${SOURCES})
target_include_directories(fileshare PRIVATE src/)
target_link_libraries(fileshare PRIVATE nlohmann_json::nlohmann_json CURL::libcurl)

if (WIN32)
	target_compile_definitions(fileshare PRIVATE NOMINMAX)
endif()