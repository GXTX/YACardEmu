cmake_minimum_required(VERSION 3.10)

project(YetAnotherCardEmulator)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SERIAL REQUIRED libserialport)

include_directories(${SERIAL_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/Includes)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/CardIo.cpp
	Includes/SerIo.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)

target_link_libraries(${PROJECT_NAME} ${SERIAL_LIBRARIES})
