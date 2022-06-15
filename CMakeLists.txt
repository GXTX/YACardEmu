cmake_minimum_required(VERSION 3.10)

project(YACardEmu)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/CardIo.cpp
	Includes/SerIo.cpp
	Includes/C1231LR.cpp
	Includes/base64.cpp
)

add_executable(printer
	printer.cpp
)

if (WIN32)
	add_subdirectory(3rdparty/libserialport)
	add_dependencies(${PROJECT_NAME} libserialport)

	set(SERIAL_INCLUDE_DIRS ${libserialport_SOURCE_DIR})
	set(SERIAL_LIBRARIES ${libserialport_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libserialport.lib)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SERIAL REQUIRED libserialport)

	find_package(ICU REQUIRED COMPONENTS i18n data uc)

	pkg_check_modules(SDL2 REQUIRED sdl2)
	pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
	pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
endif()

include_directories(
	${SERIAL_INCLUDE_DIRS}
	${THREAD_INLCUDE_DIRS}
	${PROJECT_SOURCE_DIR}/Includes
	${PROJECT_SOURCE_DIR}/3rdparty/cpp-httplib
	${PROJECT_SOURCE_DIR}/3rdparty/mINI/src
	${PROJECT_SOURCE_DIR}/3rdparty/gulfs/include
	${SDL2_INCLUDE_DIRS}
)

add_subdirectory(3rdparty/spdlog)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)
	if (supported)
		set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 11)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${PROJECT_NAME}
	${SERIAL_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	spdlog::spdlog
)

target_link_libraries(printer
	${ICU_LIBRARIES}
	${SDL2_LIBRARIES}
	${SDL2_IMAGE_LIBRARIES}
	${SDL2_TTF_LIBRARIES}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/public
		${CMAKE_CURRENT_BINARY_DIR}/public
)

if (WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			${libserialport_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libserialport.dll
			${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libserialport.dll
	)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			${CMAKE_CURRENT_SOURCE_DIR}/config.ini.sample
			${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/config.ini.sample
	)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			${CMAKE_CURRENT_SOURCE_DIR}/license.txt
			${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/license.txt
	)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/public
			${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/public
	)
endif()
