cmake_minimum_required(VERSION 3.25)
project(GeoBRHostChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT GEOBR_JSON_SOURCE)
 message(FATAL_ERROR "Pass GEOBR_JSON_SOURCE pointing to geode-sdk/json v3.3.1")
endif()
add_subdirectory(${GEOBR_JSON_SOURCE} matjson)
add_executable(shop_order_test shop_order_test.cpp)
target_link_libraries(shop_order_test PRIVATE mat-json)

target_compile_options(shop_order_test PRIVATE -UNDEBUG)

# RC6 host checks (no Geode dependency).
add_executable(arrow_physics_test ArrowPhysicsTests.cpp)
target_include_directories(arrow_physics_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_options(arrow_physics_test PRIVATE -UNDEBUG)
add_executable(text_sanitizer_test TextSanitizerTests.cpp ../src/utils/TextSanitizer.cpp)
target_compile_options(text_sanitizer_test PRIVATE -UNDEBUG)

# RC9: collab three-way merge and room header helpers.
add_executable(collab_merge_test CollabMergeTests.cpp)
target_compile_options(collab_merge_test PRIVATE -UNDEBUG)

# GDPS: which GD requests are redirected to the GeoBR server.
add_executable(gdps_routing_test GdpsRoutingTests.cpp)
target_compile_options(gdps_routing_test PRIVATE -UNDEBUG)
