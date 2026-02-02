cmake_minimum_required(VERSION 3.16)

project(automotive_bcm
    VERSION 1.0.0
    DESCRIPTION "Automotive Body Control Module"
    LANGUAGES C CXX
)

# -----------------------------------------------------------------------------
# Build Options
# -----------------------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" OFF)
option(USE_SYSTEM_CPPUTEST "Use system-installed CppUTest" ON)

# -----------------------------------------------------------------------------
# Compiler Settings
# -----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wshadow
        -Wundef
        -Wcast-align
        -Wstrict-prototypes
    )
endif()

# Debug/Release flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
set(BCM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
set(BCM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/door_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lighting_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/turn_signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fault_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/can_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/can_tx.c
)

# -----------------------------------------------------------------------------
# BCM Static Library (for linking with tests)
# -----------------------------------------------------------------------------
add_library(bcm_lib STATIC ${BCM_SOURCES})
target_include_directories(bcm_lib PUBLIC ${BCM_INCLUDE_DIRS})

# -----------------------------------------------------------------------------
# BCM Application Executable
# -----------------------------------------------------------------------------
add_executable(bcm_app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)

target_link_libraries(bcm_app PRIVATE bcm_lib)
target_include_directories(bcm_app PRIVATE ${BCM_INCLUDE_DIRS})

# -----------------------------------------------------------------------------
# Unit Tests
# -----------------------------------------------------------------------------
if(BUILD_TESTS)
    enable_testing()
    
    # Try to find system CppUTest first
    if(USE_SYSTEM_CPPUTEST)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(CPPUTEST QUIET cpputest)
        endif()
        
        # Also try direct find
        if(NOT CPPUTEST_FOUND)
            find_path(CPPUTEST_INCLUDE_DIR CppUTest/TestHarness.h
                PATHS 
                    /usr/local/include
                    /usr/include
                    /opt/homebrew/include
            )
            find_library(CPPUTEST_LIBRARY CppUTest
                PATHS
                    /usr/local/lib
                    /usr/lib
                    /opt/homebrew/lib
            )
            find_library(CPPUTEST_EXT_LIBRARY CppUTestExt
                PATHS
                    /usr/local/lib
                    /usr/lib
                    /opt/homebrew/lib
            )
            
            if(CPPUTEST_INCLUDE_DIR AND CPPUTEST_LIBRARY)
                set(CPPUTEST_FOUND TRUE)
                set(CPPUTEST_INCLUDE_DIRS ${CPPUTEST_INCLUDE_DIR})
                set(CPPUTEST_LIBRARIES ${CPPUTEST_LIBRARY} ${CPPUTEST_EXT_LIBRARY})
                message(STATUS "Found CppUTest: ${CPPUTEST_LIBRARY}")
            endif()
        endif()
    endif()
    
    # Fallback: Use FetchContent to download CppUTest
    if(NOT CPPUTEST_FOUND)
        message(STATUS "CppUTest not found locally, fetching from GitHub...")
        include(FetchContent)
        
        FetchContent_Declare(
            CppUTest
            GIT_REPOSITORY https://github.com/cpputest/cpputest.git
            GIT_TAG        v4.0
        )
        
        # Disable CppUTest's own tests and extensions we don't need
        set(TESTS OFF CACHE BOOL "" FORCE)
        set(CPPUTEST_EXTENSIONS ON CACHE BOOL "" FORCE)
        
        FetchContent_MakeAvailable(CppUTest)
        
        set(CPPUTEST_INCLUDE_DIRS ${cpputest_SOURCE_DIR}/include)
        set(CPPUTEST_LIBRARIES CppUTest CppUTestExt)
        set(CPPUTEST_FOUND TRUE)
        message(STATUS "CppUTest fetched successfully")
    endif()
    
    if(CPPUTEST_FOUND)
        add_subdirectory(tests)
    else()
        message(WARNING "CppUTest not available, tests will not be built")
    endif()
endif()

# -----------------------------------------------------------------------------
# Installation (optional)
# -----------------------------------------------------------------------------
install(TARGETS bcm_app
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/bcm
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY config/
    DESTINATION include/bcm/config
    FILES_MATCHING PATTERN "*.h"
)

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Automotive BCM Build Configuration ===")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C Standard:   ${CMAKE_C_STANDARD}")
message(STATUS "  Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests:  ${BUILD_TESTS}")
message(STATUS "==========================================")
message(STATUS "")
