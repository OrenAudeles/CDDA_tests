cmake_minimum_required(VERSION 3.9)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin)
set(LIB_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/lib)

set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

add_definitions(
	
)

include_directories(
	"${PROJECT_SOURCE_DIR}"
)

add_subdirectory(./src)
