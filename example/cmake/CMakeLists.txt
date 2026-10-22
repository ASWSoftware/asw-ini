cmake_minimum_required(VERSION 3.20)

project(ASWIniExample LANGUAGES CXX)

# The example builds at C++17 by default, the library's minimum, to show that it needs nothing newer; CI also builds it
# at C++20. When the root project builds both the tests and the example, the first of them to set this cache variable
# chooses it for both (the root adds the tests first).
set(ASWINI_CXX_STANDARD 17 CACHE STRING "The C++ standard to build the ASWIni example (and library) with: 17 or 20")
set_property(CACHE ASWINI_CXX_STANDARD PROPERTY STRINGS 17 20)
if(NOT ASWINI_CXX_STANDARD MATCHES "^(17|20)$")
    message(FATAL_ERROR "ASWINI_CXX_STANDARD must be 17 or 20 (got '${ASWINI_CXX_STANDARD}')")
endif()

set(CMAKE_CXX_STANDARD ${ASWINI_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ASWINI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# The ASWIni::ASWIni library target, from the repository root's CMakeLists.txt. It already exists when the root
# project builds the example (ASWINI_BUILD_EXAMPLE).
if(NOT TARGET ASWIni::ASWIni)
    set(ASWINI_WARNINGS ON)
    add_subdirectory("${ASWINI_ROOT}" aswini)
endif()

add_executable(ASWIniExample
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp"
)

target_link_libraries(ASWIniExample PRIVATE ASWIni::ASWIni)

# The example's second thread (std::thread) needs an explicit threading library on Linux with an older glibc and on
# some MinGW-w64 distributions.
find_package(Threads REQUIRED)
target_link_libraries(ASWIniExample PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(ASWIniExample PRIVATE /W4 /Zc:__cplusplus)
else()
    target_compile_options(ASWIniExample PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    # Statically link the MinGW runtime so the executable launches without MinGW's bin directory on PATH (e.g. on a
    # CI runner).
    target_link_options(ASWIniExample PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

set_target_properties(ASWIniExample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
