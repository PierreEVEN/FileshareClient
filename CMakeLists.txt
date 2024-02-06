cmake_minimum_required(VERSION 3.13)

include(FetchContent)

project(FileshareClient)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package (Python COMPONENTS Interpreter Development)
execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/mime-setup.py)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0 # release-1.10.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE FILESHARE_SOURCES src/fileshare/*.cpp include/fileshare/*.hpp src/fileshare/*.hpp)
add_library(fileshareClient ${FILESHARE_SOURCES})
target_link_libraries(fileshareClient PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(fileshareClient PRIVATE CURL::libcurl)
target_include_directories(fileshareClient PUBLIC include)
if (WIN32)
	target_compile_definitions(fileshareClient PUBLIC NOMINMAX)
endif()

file(GLOB_RECURSE APP_SOURCES src/app/*.cpp include/app/*.hpp src/app/*.hpp)
add_executable(fileshare ${APP_SOURCES})
target_link_libraries(fileshare PRIVATE fileshareClient)

enable_testing()

file(GLOB_RECURSE TEST_SOURCES test/*.cpp test/*.hpp)
add_executable(fileshare_tests ${TEST_SOURCES})
target_link_libraries(fileshare_tests PRIVATE GTest::gtest_main fileshareClient)

include(GoogleTest)
gtest_discover_tests(fileshare_tests)