cmake_minimum_required(VERSION 3.20)

project(ASWIniPackageCheck LANGUAGES CXX)

# Checks the installed CMake package: builds Check_Package.cpp against an ASWIni installed with `cmake --install`
# (point CMAKE_PREFIX_PATH at the install prefix), and `ctest` runs it. See the README's CMake section.
find_package(ASWIni CONFIG REQUIRED)

add_executable(ASWIniPackageCheck
    "${CMAKE_CURRENT_SOURCE_DIR}/Check_Package.cpp"
)

target_link_libraries(ASWIniPackageCheck PRIVATE ASWIni::ASWIni)

# The version the package's version file reported, which the check compares with the installed headers' version
target_compile_definitions(ASWIniPackageCheck PRIVATE "ASWINI_PACKAGE_VERSION=\"${ASWIni_VERSION}\"")

if(MSVC)
    target_compile_options(ASWIniPackageCheck PRIVATE /W4)
else()
    target_compile_options(ASWIniPackageCheck PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    # Statically link the MinGW runtime so the executable launches without MinGW's bin directory on PATH (e.g. on a
    # CI runner).
    target_link_options(ASWIniPackageCheck PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

enable_testing()
add_test(NAME ASWIniPackageCheck COMMAND ASWIniPackageCheck)
