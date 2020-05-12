cmake_minimum_required(VERSION 3.10)

project(WMMT)

include_directories(${PROJECT_SOURCE_DIR}/Includes)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/JvsIo.cpp
	Includes/SerIo.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
)

target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)
