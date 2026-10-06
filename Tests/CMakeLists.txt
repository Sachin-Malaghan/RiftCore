# ============================================================
# Tests - run with:  ctest --test-dir Build -C Release --output-on-failure
#
# The engine is tested end to end through the headless Runtime: a Python
# script builds a scene, checks the scripting API, saves / reloads it and
# verifies the physics result after three simulated seconds.
# ============================================================

add_test(NAME headless_boot
    COMMAND RiftCoreRuntime --headless --frames 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(headless_boot PROPERTIES
    PASS_REGULAR_EXPRESSION "Simulated 60 frames")

add_test(NAME scripting_and_physics
    COMMAND RiftCoreRuntime --headless --scene "" --frames 240
            --script Tests/test_scripting.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(scripting_and_physics PROPERTIES
    PASS_REGULAR_EXPRESSION "TEST PASSED"
    FAIL_REGULAR_EXPRESSION "Traceback;Python is not available")

add_test(NAME house_designer
    COMMAND RiftCoreRuntime --headless --scene "" --script Tests/test_housegen.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(house_designer PROPERTIES
    PASS_REGULAR_EXPRESSION "HOUSEGEN PASSED"
    FAIL_REGULAR_EXPRESSION "Traceback;Python is not available")

message(STATUS "[Tests] 3 headless tests registered (ctest).")
