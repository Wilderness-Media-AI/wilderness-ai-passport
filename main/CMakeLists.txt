set(APP_SRCS
    "main.c"
    "badge_idle_logic.c"
    "badge_voice_logic.c"
    "badge_audio_owner.c"
    "badge_adpcm.c"
    "badge_ble_mic.c"
    "wilderness_badge.c"
    "wilderness_logo.c"
    "wilderness_services_cn.c"
)
set(APP_INCLUDE_DIRS ".")

# The top-level CMake resolves and validates the profile before ESP-IDF starts
# its child component-requirements pass, then exports that exact choice here.
set(BADGE_PROFILE_DIR "$ENV{BADGE_PROFILE_DIR_RESOLVED}")
if(NOT BADGE_PROFILE_DIR STREQUAL "")
    list(APPEND APP_SRCS "${BADGE_PROFILE_DIR}/badge_avatar_private.c")
    list(PREPEND APP_INCLUDE_DIRS "${BADGE_PROFILE_DIR}")
    set(BADGE_PROFILE_ACTIVE TRUE)
elseif(NOT "$ENV{BADGE_GENERIC_BUILD_RESOLVED}" STREQUAL "ON")
    message(FATAL_ERROR
            "Badge profile resolution was not propagated from top-level CMake")
endif()

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS ${APP_INCLUDE_DIRS}
    REQUIRES bsp
    PRIV_REQUIRES esp_timer bt nvs_flash
)

if(BADGE_PROFILE_ACTIVE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BADGE_PROFILE_ACTIVE=1)
endif()
