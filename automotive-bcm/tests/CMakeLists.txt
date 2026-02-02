# =============================================================================
# BCM Unit Tests CMake Configuration
# =============================================================================

set(TEST_SOURCES
    test_door_control.cpp
    test_lighting_control.cpp
    test_fault_manager.cpp
    test_main.cpp
)

# Create test executable
add_executable(bcm_tests ${TEST_SOURCES})

# Link against BCM library and CppUTest
target_link_libraries(bcm_tests
    PRIVATE
        bcm_lib
        ${CPPUTEST_LIBRARIES}
)

# Include directories
target_include_directories(bcm_tests
    PRIVATE
        ${BCM_INCLUDE_DIRS}
        ${CPPUTEST_INCLUDE_DIRS}
)

# C++ standard for tests
set_target_properties(bcm_tests PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# Suppress some warnings for test code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bcm_tests PRIVATE
        -Wno-conversion
        -Wno-sign-conversion
    )
endif()

# Register tests with CTest
add_test(NAME bcm_unit_tests COMMAND bcm_tests)
add_test(NAME bcm_unit_tests_verbose COMMAND bcm_tests -v)

# Custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS bcm_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running BCM unit tests..."
)
