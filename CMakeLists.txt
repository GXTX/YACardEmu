cmake_minimum_required(VERSION 3.10)

project(YACardEmu)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SERIAL REQUIRED libserialport)

include_directories(${SERIAL_INCLUDE_DIRS} ${THREAD_INLCUDE_DIRS}
	${PROJECT_SOURCE_DIR}/Includes
	${PROJECT_SOURCE_DIR}/3rdparty/cpp-httplib
	${PROJECT_SOURCE_DIR}/3rdparty/mINI/src
)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/CardIo.cpp
	Includes/SerIo.cpp
	Includes/C1231LR.cpp
	Includes/base64.cpp
)

add_subdirectory(3rdparty/spdlog)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)
	if (supported)
		set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.9)
	target_link_libraries(${PROJECT_NAME} ${SERIAL_LIBRARIES}
		${CMAKE_THREAD_LIBS_INIT}
		spdlog::spdlog
	)
else ()
	target_link_libraries(${PROJECT_NAME} ${SERIAL_LIBRARIES}
		${CMAKE_THREAD_LIBS_INIT}
		stdc++fs #gcc8 has odd support for this
		spdlog::spdlog
	)
endif()

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/site
                ${CMAKE_CURRENT_BINARY_DIR}/site
)
