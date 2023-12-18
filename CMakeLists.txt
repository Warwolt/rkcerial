cmake_minimum_required(VERSION 3.24)
set(AVR_UPLOADTOOL avrdude)
set(AVR_PROGRAMMER arduino)
option(AVR_UPLOADTOOL_PORT "USB port that device is connected to")
set(AVR_MCU atmega328p)
set(MCU_SPEED 16000000UL)
project(rkcerial)
enable_language(C)

# ------------------------- Cross-compilation setup -------------------------- #
if(AVR)
    if (AVR_UPLOADTOOL_PORT MATCHES "OFF")
        message(FATAL_ERROR
            "AVR_UPLOADTOOL_PORT is required but not defined. "
            "Please define it using -DAVR_UPLOADTOOL_PORT=<usb port>, "
            "where <usb port> is the port your device is connected to: "
            "e.g. com5 (windows), /dev/ttyUSB0 (linux)"
        )
    endif()

    if(DEFINED ENV{AVR_FIND_ROOT_PATH})
        set(CMAKE_FIND_ROOT_PATH $ENV{AVR_FIND_ROOT_PATH})
    else(DEFINED ENV{AVR_FIND_ROOT_PATH})
        message(FATAL_ERROR "Please set AVR_FIND_ROOT_PATH in your environment.")
    endif(DEFINED ENV{AVR_FIND_ROOT_PATH})

    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    # not added automatically, since CMAKE_SYSTEM_NAME is "generic"
    set(CMAKE_SYSTEM_INCLUDE_PATH "${CMAKE_FIND_ROOT_PATH}/include")
    set(CMAKE_SYSTEM_LIBRARY_PATH "${CMAKE_FIND_ROOT_PATH}/lib")


    if(CMAKE_BUILD_TYPE MATCHES Release)
        set(CMAKE_C_FLAGS_RELEASE "-Os")
        set(CMAKE_CXX_FLAGS_RELEASE "-Os")
    endif(CMAKE_BUILD_TYPE MATCHES Release)

    if(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -save-temps -g -gdwarf-3 -gstrict-dwarf")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -save-temps -g -gdwarf-3 -gstrict-dwarf")
    endif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)

    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(CMAKE_C_FLAGS_DEBUG "-O0 -save-temps -g -gdwarf-3 -gstrict-dwarf")
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -save-temps -g -gdwarf-3 -gstrict-dwarf")
    endif(CMAKE_BUILD_TYPE MATCHES Debug)
endif(AVR)

# --------------------------- Report configuration --------------------------- #
if(AVR)
    message(STATUS "Current uploadtool is: ${AVR_UPLOADTOOL}")
    message(STATUS "Current programmer is: ${AVR_PROGRAMMER}")
    message(STATUS "Current upload port is: ${AVR_UPLOADTOOL_PORT}")
    message(STATUS "Current uploadtool options are: ${AVR_UPLOADTOOL_OPTIONS}")
    message(STATUS "Current MCU is set to: ${AVR_MCU}")
    message(STATUS "Set CMAKE_FIND_ROOT_PATH to ${CMAKE_FIND_ROOT_PATH}")
    message(STATUS "Set CMAKE_SYSTEM_INCLUDE_PATH to ${CMAKE_SYSTEM_INCLUDE_PATH}")
    message(STATUS "Set CMAKE_SYSTEM_LIBRARY_PATH to ${CMAKE_SYSTEM_LIBRARY_PATH}")
endif(AVR)

# ---------------------------- Program executable ---------------------------- #
add_avr_library(${PROJECT_NAME}
    src/logging.c
)
add_avr_executable("${PROJECT_NAME}_main"
   src/main.c
)
avr_target_include_directories(${PROJECT_NAME} PUBLIC
    include
)
avr_target_link_libraries("${PROJECT_NAME}_main" PRIVATE ${PROJECT_NAME})
add_definitions("-DF_CPU=${MCU_SPEED}")
add_compile_options(-Os)

