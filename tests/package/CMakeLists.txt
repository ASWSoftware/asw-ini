cmake_minimum_required(VERSION 3.20)

project(ASWIniPackageCheck LANGUAGES CXX)

# Checks the installed CMake package: builds Check_Package.cpp (the version) and Check_Usage.cpp (every header, a
# settings class, a text and a file) against an ASWIni installed with `cmake --install` (point CMAKE_PREFIX_PATH at
# the install prefix), and `ctest` runs them. See the README's CMake section.
find_package(ASWIni CONFIG REQUIRED)

add_executable(ASWIniPackageCheck
    "${CMAKE_CURRENT_SOURCE_DIR}/Check_Package.cpp"
)

add_executable(ASWIniUsageCheck
    "${CMAKE_CURRENT_SOURCE_DIR}/Check_Usage.cpp"
)

# The version the package's version file reported, which the check compares with the installed headers' version
target_compile_definitions(ASWIniPackageCheck PRIVATE "ASWINI_PACKAGE_VERSION=\"${ASWIni_VERSION}\"")

foreach(check ASWIniPackageCheck ASWIniUsageCheck)
    target_link_libraries(${check} PRIVATE ASWIni::ASWIni)

    if(MSVC)
        target_compile_options(${check} PRIVATE /W4)
    else()
        target_compile_options(${check} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(MINGW)
        # Statically link the MinGW runtime so the executable launches without MinGW's bin directory on PATH (e.g. on
        # a CI runner).
        target_link_options(${check} PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()
endforeach()

enable_testing()
add_test(NAME ASWIniPackageCheck COMMAND ASWIniPackageCheck)
add_test(NAME ASWIniUsageCheck COMMAND ASWIniUsageCheck)
