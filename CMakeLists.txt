cmake_minimum_required(VERSION 3.11)

project(YACardEmu)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc)
pkg_check_modules(SERIAL REQUIRED libserialport)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

include_directories(${SERIAL_INCLUDE_DIRS} ${THREAD_INLCUDE_DIRS}
	${PROJECT_SOURCE_DIR}/Includes
	${PROJECT_SOURCE_DIR}/3rdparty/cpp-httplib
	${PROJECT_SOURCE_DIR}/3rdparty/mINI/src
)

add_executable(${PROJECT_NAME}
	main.cpp
	Includes/CardIo.cpp
	Includes/SerIo.cpp
	Includes/ICUConv.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)
	if (supported)
		set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

add_subdirectory(3rdparty/spdlog)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_compile_options(${PROJECT_NAME} PRIVATE -march=native -Wall -Wextra -Wpedantic)

target_link_libraries(${PROJECT_NAME} ${SERIAL_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	${ICU_LIBRARIES}
	${SDL2_LIBRARIES}
	${SDL2_IMAGE_LIBRARIES}
	${SDL2_TTF_LIBRARIES}
	spdlog::spdlog
)

include(FetchContent)

FetchContent_Declare(
	font
	URL https://unifoundry.com/pub/unifont/unifont-14.0.02/font-builds/unifont_jp-14.0.02.ttf
	DOWNLOAD_NO_EXTRACT true
)

FetchContent_MakeAvailable(font)

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/site
                ${CMAKE_CURRENT_BINARY_DIR}/site
)

add_custom_command(TARGET ${PROJECT_NAME}  POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/_deps/font-src/unifont_jp-14.0.02.ttf
                ${CMAKE_CURRENT_BINARY_DIR}/unifont_jp.ttf
)
