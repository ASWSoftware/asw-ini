cmake_minimum_required(VERSION 3.20)

project(ASWIniTests LANGUAGES CXX)

# The library needs C++17 (RAD Studio's bcc32c); the tests build at C++20 by default, which also compiles the optional
# C++20 additions. CI builds both, so a C++17 build catches C++20 code that the C++17 path must not use.
set(ASWINI_CXX_STANDARD 20 CACHE STRING "The C++ standard to build the ASWIni unit tests (and library) with: 17 or 20")
set_property(CACHE ASWINI_CXX_STANDARD PROPERTY STRINGS 17 20)
if(NOT ASWINI_CXX_STANDARD MATCHES "^(17|20)$")
    message(FATAL_ERROR "ASWINI_CXX_STANDARD must be 17 or 20 (got '${ASWINI_CXX_STANDARD}')")
endif()

set(CMAKE_CXX_STANDARD ${ASWINI_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ASWINI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(ASWINI_TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
set(ASWUNITTESTS_ROOT "${ASWINI_ROOT}/third_party/asw-unit-tests")

# The ASWUnitTests framework is a git submodule. Fail with instructions rather than falling back to a download, so a
# build (CI included) always uses exactly the commit the submodule pins.
if(NOT EXISTS "${ASWUNITTESTS_ROOT}/src/ASWUnitTests_Sources.cmake")
    message(FATAL_ERROR
        "The ASWUnitTests submodule is missing from: ${ASWUNITTESTS_ROOT}\n"
        "From the repository root, run:\n"
        "    git submodule update --init\n"
        "or clone with:\n"
        "    git clone --recurse-submodules https://github.com/ASWSoftware/asw-ini.git")
endif()

include("${ASWUNITTESTS_ROOT}/src/ASWUnitTests_Sources.cmake")

# The ASWIni::ASWIni library target, from the repository root's CMakeLists.txt. It already exists when the root
# project builds the tests (ASWINI_BUILD_TESTS).
if(NOT TARGET ASWIni::ASWIni)
    set(ASWINI_WARNINGS ON)
    add_subdirectory("${ASWINI_ROOT}" aswini)
endif()

add_executable(ASWIniTests
    ${ASWUNITTESTS_SOURCES}
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Converters.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Document.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_File.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Options.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Platform.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Types.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Utils.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Version.cpp"
    "${ASWINI_TESTS_DIR}/UT_Helper_TempDir.cpp"
)

target_include_directories(ASWIniTests PRIVATE
    "${ASWUNITTESTS_SOURCE_DIR}"
    "${ASWINI_TESTS_DIR}"
)

target_link_libraries(ASWIniTests PRIVATE ASWIni::ASWIni)

# ASWUnitTests' --test-timeout-seconds uses std::thread/std::future, which needs an explicit threading library on
# Linux and some MinGW-w64 distributions.
find_package(Threads REQUIRED)
target_link_libraries(ASWIniTests PRIVATE Threads::Threads)

if(MSVC)
    target_compile_options(ASWIniTests PRIVATE /W4 /Zc:__cplusplus)
else()
    target_compile_options(ASWIniTests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    # Statically link the MinGW runtime so the executable launches without MinGW's bin directory on PATH (e.g. on a
    # CI runner).
    target_link_options(ASWIniTests PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

set_target_properties(ASWIniTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
