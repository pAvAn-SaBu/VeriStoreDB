cmake_minimum_required(VERSION 3.14...4.3)
project(vsdb VERSION 1.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download CLI11
include(FetchContent)
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11)

# Source files
set(SOURCES
    src/main.cpp
    src/cli/cli_parser.cpp
    src/db/database.cpp
    src/gitstore/gitstore.cpp
)

# Create executable
add_executable(vsdb ${SOURCES})

# Link CLI11
target_link_libraries(vsdb PRIVATE CLI11::CLI11)

# Include directories
target_include_directories(vsdb PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link filesystem library if needed (for older GCC versions)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(vsdb PRIVATE stdc++fs)
endif()