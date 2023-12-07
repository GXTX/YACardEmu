cmake_minimum_required(VERSION 3.10)

project(YACardEmu)
cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/CardIo.cpp
	Includes/SerIo.cpp
	Includes/C1231LR.cpp
	Includes/C1231BR.cpp
	Includes/base64.cpp
	Includes/WebIo.cpp
	Includes/Printer.cpp
)

if (WIN32)
	add_subdirectory(3rdparty/libserialport)
	set(SERIAL_INCLUDE_DIRS ${libserialport_SOURCE_DIR})
	add_subdirectory(3rdparty/libiconv)

	set(SDL_ATOMIC FALSE)
	set(SDL_AUDIO FALSE)
	set(SDL_VIDEO FALSE)
	set(SDL_RENDER FALSE)
	set(SDL_EVENTS FALSE)
	set(SDL_JOYSTICK FALSE)
	set(SDL_HAPTIC FALSE)
	set(SDL_HIDAPI FALSE)
	set(SDL_POWER FALSE)
	#set(SDL_THREADS FALSE)
	set(SDL_TIMERS FALSE)
	set(SDL_FILESYSTEM FALSE)
	#set(SDL_LOADSO FALSE)
	set(SDL_CPUINFO FALSE)
	set(SDL_SENSOR FALSE)
	#set(SDL_LOCALE FALSE)
	set(SDL_INSTALL FALSE)
	set(SDL_TESTS FALSE)
	set(BUILD_SHARED_LIBS FALSE)
	set(SDL2TTF_SAMPLES FALSE)
	set(SDL2TTF_INSTALL FALSE)
	set(SDL2TTF_TESTS FALSE)
	set(SDL2IMAGE_INSTALL FALSE)
	set(SDL2IMAGE_SAMPLES FALSE)
	set(SDL2IMAGE_TESTS FALSE)
	add_subdirectory(3rdparty/SDL)
	add_subdirectory(3rdparty/SDL2_image)
	add_subdirectory(3rdparty/SDL2_ttf)
	target_compile_definitions(SDL2-static PRIVATE HAVE_ICONV=1 HAVE_ICONV_H=1)
	target_link_libraries(SDL2-static PRIVATE libiconv)
	target_include_directories(SDL2-static PUBLIC "${PROJECT_SOURCE_DIR}/3rdparty/libiconv/include")
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SERIAL REQUIRED libserialport)
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
	${PROJECT_SOURCE_DIR}/3rdparty/utf8
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

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 14)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
	add_definitions(-DWIN32_LEAN_AND_MEAN -DLIBSERIALPORT_MSBUILD -DLIBSERIALPORT_STATIC)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)
endif()

if (MSVC)
	target_link_libraries(${PROJECT_NAME}
		${CMAKE_THREAD_LIBS_INIT}
		spdlog::spdlog
		libserialport
		SDL2::SDL2main
		SDL2::SDL2-static
		SDL2_ttf::SDL2_ttf-static
		SDL2_image::SDL2_image-static
	)
else()
	target_link_libraries(${PROJECT_NAME}
		${SERIAL_LIBRARIES}
		${CMAKE_THREAD_LIBS_INIT}
		spdlog::spdlog
		${SDL2_LIBRARIES}
		${SDL2_IMAGE_LIBRARIES}
		${SDL2_TTF_LIBRARIES}
	)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/public
		${CMAKE_CURRENT_BINARY_DIR}/public
)

if (WIN32)
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
