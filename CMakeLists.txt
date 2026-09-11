cmake_minimum_required(VERSION 3.16)

option(BADGE_ALLOW_GENERIC
       "Allow a generic non-employee build without a private badge profile"
       OFF)

# ESP-IDF evaluates component CMakeLists files in a child requirements pass.
# Validate the employee profile here, then export the resolved choice so that
# both the requirements pass and the real build see exactly the same profile.
if(DEFINED BADGE_PROFILE_DIR AND NOT BADGE_PROFILE_DIR STREQUAL "")
    get_filename_component(BADGE_PROFILE_DIR "${BADGE_PROFILE_DIR}" ABSOLUTE
                           BASE_DIR "${CMAKE_SOURCE_DIR}")
    foreach(REQUIRED_PROFILE_FILE
            badge_profile_private.h
            badge_avatar_private.h
            badge_avatar_private.c
            wechat_qr_private.h)
        if(NOT EXISTS "${BADGE_PROFILE_DIR}/${REQUIRED_PROFILE_FILE}")
            message(FATAL_ERROR
                    "Badge profile is incomplete: ${BADGE_PROFILE_DIR}/${REQUIRED_PROFILE_FILE}")
        endif()
    endforeach()
    set(ENV{BADGE_PROFILE_DIR_RESOLVED} "${BADGE_PROFILE_DIR}")
    set(ENV{BADGE_GENERIC_BUILD_RESOLVED} "OFF")
    message(STATUS "Building private badge profile from ${BADGE_PROFILE_DIR}")
elseif(NOT BADGE_ALLOW_GENERIC)
    message(FATAL_ERROR
            "Employee badge build requires -D BADGE_PROFILE_DIR=/absolute/path/to/private-profiles/<slug>. "
            "Use -D BADGE_ALLOW_GENERIC=ON only for CI/community validation; never flash that build to an employee device.")
else()
    set(ENV{BADGE_PROFILE_DIR_RESOLVED} "")
    set(ENV{BADGE_GENERIC_BUILD_RESOLVED} "ON")
    message(STATUS "Building explicitly authorized generic badge firmware")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(Wilderness-AI-Passport)
