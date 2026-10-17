cmake_minimum_required(VERSION 3.20)

# The version lives only in ASWIni/ASWIni_Version.h, so the project version comes from its ASWINI_VERSION_MAJOR,
# _MINOR, and _PATCH macros (a CMake version has no pre-release part).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/ASWIni/ASWIni_Version.h" ASWINI_VERSION_DEFINES
    REGEX "^#define ASWINI_VERSION_(MAJOR|MINOR|PATCH) ")
foreach(versionPart MAJOR MINOR PATCH)
    if(NOT ASWINI_VERSION_DEFINES MATCHES "#define ASWINI_VERSION_${versionPart} ([0-9]+)")
        message(FATAL_ERROR "ASWIni/ASWIni_Version.h doesn't define ASWINI_VERSION_${versionPart} as a number")
    endif()
    set(ASWINI_VERSION_${versionPart} "${CMAKE_MATCH_1}")
endforeach()

project(ASWIni
    VERSION ${ASWINI_VERSION_MAJOR}.${ASWINI_VERSION_MINOR}.${ASWINI_VERSION_PATCH}
    DESCRIPTION "A thread-safe, comment-preserving C++17 INI file reader/writer for Windows and Linux"
    HOMEPAGE_URL "https://github.com/ASWSoftware/asw-ini"
    LANGUAGES CXX
)

# Building ASWIni itself (cmake -S .) rather than consuming it through add_subdirectory() or FetchContent.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(ASWINI_IS_TOP_LEVEL ON)
else()
    set(ASWINI_IS_TOP_LEVEL OFF)
endif()

option(ASWINI_BUILD_TESTS "Build the ASWIni unit tests (tests/cmake; needs the asw-unit-tests submodule)" OFF)
option(ASWINI_INSTALL "Generate the install rules for the ASWIni library, headers, and CMake package files"
    ${ASWINI_IS_TOP_LEVEL})
option(ASWINI_WARNINGS "Compile the ASWIni sources with extra warnings" ${ASWINI_IS_TOP_LEVEL})

set(ASWINI_SANITIZE "" CACHE STRING
    "Build ASWIni and everything that links to it with a sanitizer: thread, or address (with undefined behavior)")
set_property(CACHE ASWINI_SANITIZE PROPERTY STRINGS "" thread address)

add_library(ASWIni STATIC
    ASWIni/ASWIni_File.cpp
    ASWIni/ASWIni_File.h
    ASWIni/ASWIni_Options.cpp
    ASWIni/ASWIni_Options.h
    ASWIni/ASWIni_Types.cpp
    ASWIni/ASWIni_Types.h
    ASWIni/ASWIni_Utils.cpp
    ASWIni/ASWIni_Utils.h
    ASWIni/ASWIni_Version.cpp
    ASWIni/ASWIni_Version.h
)
add_library(ASWIni::ASWIni ALIAS ASWIni)

# C++17 is the minimum (RAD Studio's 32-bit compiler, bcc32c, supports only C++17). A consumer may compile at C++20
# or later, which turns on the optional C++20 additions.
target_compile_features(ASWIni PUBLIC cxx_std_17)
# A Debug build is named ASWInid, so Debug and Release can be installed to the same prefix.
set_target_properties(ASWIni PROPERTIES
    CXX_EXTENSIONS OFF
    DEBUG_POSTFIX d
)

# The installed target's include folder comes from install(TARGETS) below.
target_include_directories(ASWIni PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ASWIni>")

if(ASWINI_WARNINGS)
    if(MSVC)
        target_compile_options(ASWIni PRIVATE /W4)
    else()
        target_compile_options(ASWIni PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# A sanitizer must instrument the whole program (ThreadSanitizer misses races in, or misreports them through,
# uninstrumented code), so the flags are PUBLIC: they reach the compile and link of every target linking to ASWIni.
if(ASWINI_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "ASWINI_SANITIZE is supported only with GCC and Clang")
    endif()

    if(ASWINI_SANITIZE STREQUAL "thread")
        set(ASWINI_SANITIZE_FLAGS -fsanitize=thread)
    elseif(ASWINI_SANITIZE STREQUAL "address")
        # Stop at the first undefined behavior report, as AddressSanitizer does, so the run fails.
        set(ASWINI_SANITIZE_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined)
    else()
        message(FATAL_ERROR "ASWINI_SANITIZE must be empty, thread, or address (got '${ASWINI_SANITIZE}')")
    endif()

    # Debug information and frame pointers give the sanitizer reports complete call stacks.
    target_compile_options(ASWIni PUBLIC ${ASWINI_SANITIZE_FLAGS} -g -fno-omit-frame-pointer)
    target_link_options(ASWIni PUBLIC ${ASWINI_SANITIZE_FLAGS})
endif()

# After `cmake --install`, find_package(ASWIni CONFIG) gives the ASWIni::ASWIni target. The headers are installed to
# <prefix>/include/ASWIni, the installed target's include folder, so code includes them the same way as from the
# sources (e.g. #include "ASWIni_Version.h").
if(ASWINI_INSTALL)
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    set(ASWINI_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/ASWIni")

    install(TARGETS ASWIni
        EXPORT ASWIniTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ASWIni"
    )

    # Every header is public.
    get_target_property(ASWINI_HEADERS ASWIni SOURCES)
    list(FILTER ASWINI_HEADERS INCLUDE REGEX "\\.h$")
    install(FILES ${ASWINI_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ASWIni")

    install(EXPORT ASWIniTargets
        NAMESPACE ASWIni::
        DESTINATION "${ASWINI_INSTALL_CMAKEDIR}"
    )

    configure_package_config_file(cmake/ASWIniConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/ASWIniConfig.cmake"
        INSTALL_DESTINATION "${ASWINI_INSTALL_CMAKEDIR}"
    )

    # As in Semantic Versioning, find_package(ASWIni 1.2) accepts version 1.2.0 or any later 1.x, but not 2.0.
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/ASWIniConfigVersion.cmake"
        VERSION "${ASWIni_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ASWIniConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ASWIniConfigVersion.cmake"
        DESTINATION "${ASWINI_INSTALL_CMAKEDIR}"
    )
endif()

if(ASWINI_BUILD_TESTS)
    add_subdirectory(tests/cmake tests)
endif()
