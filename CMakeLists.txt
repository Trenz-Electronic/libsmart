cmake_minimum_required(VERSION 3.0)
project(libsmart CXX)

# Set the version information here
set(MAJOR_VERSION 1)
set(LIBVER ${MAJOR_VERSION}.0.0)

find_package(PkgConfig)
find_package(Threads)
# FIXME: how to check for libcrack2?

# libsmart
file(GLOB smart_includes smart/*.h)
install(FILES ${smart_includes} DESTINATION include/smart)

file(GLOB smart_includes_hw smart/hw/*.h)
install(FILES ${smart_includes_hw} DESTINATION include/smart/hw)

file(GLOB smart_includes_ts smart/ts/*.h)
install(FILES ${smart_includes_ts} DESTINATION include/smart/ts)

file(GLOB_RECURSE smart_sources smart/*.cpp)
add_library(smart SHARED ${smart_sources})

target_link_libraries(smart ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(smart PROPERTIES DEFINE_SYMBOL "smart_EXPORTS")
#set_target_properties(smart PROPERTIES OUTPUT_NAME smart)
set_target_properties(smart PROPERTIES SOVERSION ${MAJOR_VERSION})
set_target_properties(smart PROPERTIES VERSION ${LIBVER})


# uio
file(GLOB uio_sources src_uio/*.cpp)
add_executable(uio ${uio_sources} )
target_link_libraries(uio smart crack crypt m)
target_compile_options(uio PUBLIC -I${CMAKE_CURRENT_SOURCE_DIR})

