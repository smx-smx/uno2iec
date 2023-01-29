cmake_minimum_required(VERSION 3.7)
project(commandline CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

include_directories(${CMAKE_BINARY_DIR})
add_subdirectory(assembly)

add_library(utils utils.cc)
add_library(drive_factory drive_factory.cc)
add_library(image_drive_d64 image_drive_d64.cc)

add_library(cbm1541_drive cbm1541_drive.cc)
add_dependencies(cbm1541_drive format_h rw_block_h)

target_link_libraries(drive_factory cbm1541_drive image_drive_d64)

add_library(iec_host
	iec_host_lib.cc
)

add_executable(disccopy disccopy.cc)
target_link_libraries(disccopy
	drive_factory
	iec_host utils
	Threads::Threads
	${Boost_LIBRARIES}
)