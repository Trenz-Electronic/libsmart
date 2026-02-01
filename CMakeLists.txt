cmake_minimum_required(VERSION 3.10)
project(libsmart CXX)

include(GNUInstallDirs) # for the CMAKE_INSTALL_XXX to work

# Set the version information here
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)
set(CPACK_DEB_COMPONENT_INSTALL ON)

set(LIBVER ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})

set(CPACK_PACKAGE_VENDOR "Trenz Electronic GmbH")
set(CPACK_PACKAGE_CONTACT "a.errapart@trenz-electronic.de")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Andrei Errapart <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++ routines for UIO device management, WAV file handling and more"
    CACHE STRING "C++ routines for UIO device management, WAV file reading and writing, Process creation, Thread management and simply for reading and writingfiles"
)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT) # otherwise we will get _Linux.deb file and not _amd64.deb.
set(CPACK_STRIP_FILES YES)
# FIXME: correct interdependency problems
#set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS YES)
set(CPACK_COMPONENTS_GROUPING IGNORE) # one package per component


# Package: libsmart
set(CPACK_DEBIAN_SMART_PACKAGE_NAME "libsmart")
set(CPACK_DEBIAN_SMART_PACKAGE_SHLIBDEPS YES)


# Package: libsmart-dev
set(CPACK_DEBIAN_SMARTDEV_PACKAGE_NAME "libsmart-dev")
set(CPACK_DEBIAN_SMARTDEV_SHLIBDEPS YES)

# Package: uio
set(CPACK_DEBIAN_TOOLS_PACKAGE_NAME "libsmart-tools")
set(CPACK_DEBIAN_TOOLS_DESCRIPTION¶ "Tools and sample programs based on libsmart")
# TODO: how to avoid SHLIBDEPS failing to find our own libraries?


find_package(PkgConfig)
find_package(Threads)

# Testing
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.0
    )
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    add_subdirectory(tests)
endif()

# Requirements
# TODO: how to check for libcrack2?

# libsmart
file(GLOB smart_includes smart/*.h)
install(FILES ${smart_includes} DESTINATION include/smart COMPONENT smartdev)

file(GLOB smart_includes_hw smart/hw/*.h)
install(FILES ${smart_includes_hw} DESTINATION include/smart/hw COMPONENT smartdev)

file(GLOB smart_includes_ts smart/ts/*.h)
install(FILES ${smart_includes_ts} DESTINATION include/smart/ts COMPONENT smartdev)

file(GLOB_RECURSE smart_sources smart/*.cpp)
add_library(smart SHARED ${smart_sources})

target_compile_features(smart PUBLIC cxx_std_20)  # C++ 20 adds coroutines.
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
  target_compile_options(smart PUBLIC "-fcoroutines")
endif()
target_link_libraries(smart ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(smart PROPERTIES DEFINE_SYMBOL "smart_EXPORTS")
#set_target_properties(smart PROPERTIES OUTPUT_NAME smart)
set_target_properties(smart PROPERTIES SOVERSION ${CPACK_PACKAGE_VERSION_MAJOR})
set_target_properties(smart PROPERTIES VERSION ${LIBVER})
install(TARGETS smart LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT smart)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix \${prefix})
set(libdir \${exec_prefix})
set(includedir \${prefix}/include)

CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/libsmart.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libsmart.pc
@ONLY)

INSTALL(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libsmart.pc
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig"
    COMPONENT smartdev
)

# Apps
add_subdirectory(apps)

# Debian packages
include(CPack) # this must come after all install statements.
cpack_add_component(smart)
cpack_add_component(smartdev DEPENDS smart)
cpack_add_component(tools DEPENDS smart)
get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)

