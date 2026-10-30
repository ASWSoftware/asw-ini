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

# The test modules and helpers, which both the console runner and, on Windows, the Win32 GUI runner run (see
# ASWIniTestObjects below).
set(ASWINI_TEST_SOURCES
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Base.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Converters.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Document.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_File.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Interface.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Options.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Platform.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Settings.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Types.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Utils.cpp"
    "${ASWINI_TESTS_DIR}/Test_ASWIni_Version.cpp"
    "${ASWINI_TESTS_DIR}/UT_Helper_FailingAlloc.cpp"
    "${ASWINI_TESTS_DIR}/UT_Helper_TempDir.cpp"
    "${ASWINI_TESTS_DIR}/UT_Helper_Threads.cpp"
)

# ASWUnitTests' --test-timeout-seconds uses std::thread/std::future, which needs an explicit threading library on
# Linux and some MinGW-w64 distributions.
find_package(Threads REQUIRED)

# The warnings the tests and the test runners compile with.
function(aswini_set_test_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# The tests, compiled once for both test runners. An object library, whose object files are all linked into each
# runner, rather than a static library, from which a linker only takes the object files something refers to: nothing
# refers to a test module, since each registers itself, so every test would be left out.
add_library(ASWIniTestObjects OBJECT ${ASWINI_TEST_SOURCES})

target_include_directories(ASWIniTestObjects PRIVATE
    "${ASWUNITTESTS_SOURCE_DIR}"
    "${ASWINI_TESTS_DIR}"
)

# PUBLIC, so the runners link them too, and compile their own sources with ASWIni's options (ASWINI_SANITIZE's).
target_link_libraries(ASWIniTestObjects PUBLIC ASWIni::ASWIni Threads::Threads)
aswini_set_test_warnings(ASWIniTestObjects)

# The settings both test runners share.
function(aswini_configure_test_runner target)
    target_link_libraries(${target} PRIVATE ASWIniTestObjects)
    aswini_set_test_warnings(${target})

    if(MINGW)
        # Statically link the MinGW runtime so the executable launches without MinGW's bin directory on PATH (e.g. on
        # a CI runner).
        target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()

add_executable(ASWIniTests ${ASWUNITTESTS_SOURCES})

target_include_directories(ASWIniTests PRIVATE "${ASWUNITTESTS_SOURCE_DIR}")
aswini_configure_test_runner(ASWIniTests)

if(WIN32)
    # The ASWUnitTests icon, which the resource compiler finds through the include path.
    target_sources(ASWIniTests PRIVATE "${ASWUNITTESTS_ROOT}/resource/ASWUnitTests_Icon.rc")
    target_include_directories(ASWIniTests PRIVATE "${ASWUNITTESTS_ROOT}/resource")

    # The Win32 GUI runner: the same tests in a window. Its WinMain takes the place of the console runner's main.cpp,
    # and its sources list has the icon.
    include("${ASWUNITTESTS_ROOT}/win32/gui/src/ASWUnitTests_Win32GUI_Sources.cmake")

    add_executable(ASWIniTests_Win32_GUI WIN32 ${ASWUNITTESTS_WIN32_GUI_SOURCES})

    target_include_directories(ASWIniTests_Win32_GUI PRIVATE ${ASWUNITTESTS_WIN32_GUI_INCLUDE_DIRS})
    target_link_libraries(ASWIniTests_Win32_GUI PRIVATE ${ASWUNITTESTS_WIN32_GUI_LIBRARIES})
    target_link_options(ASWIniTests_Win32_GUI PRIVATE ${ASWUNITTESTS_WIN32_GUI_LINK_OPTIONS})
    aswini_configure_test_runner(ASWIniTests_Win32_GUI)
endif()
