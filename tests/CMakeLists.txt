# Tests (run with: ctest --test-dir <build folder>). The tests that compute geomGMD need a Gurobi licence;
# without one they are reported as skipped.

set(TINY_GRAPHS ${PROJECT_SOURCE_DIR}/examples/tiny/tiny_graphs.json)

function(geomgmd_add_test name library)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE ${library})
  geomgmd_enable_warnings(${name})
  add_test(NAME ${name} COMMAND ${name} ${ARGN} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
endfunction()

geomgmd_add_test(test_geometric_graph geomGMD_io ${TINY_GRAPHS})
geomgmd_add_test(test_angles geomGMD_utility)
geomgmd_add_test(test_mapping_cost geomGMD_io ${TINY_GRAPHS})
geomgmd_add_test(test_config geomGMD_io)
geomgmd_add_test(test_geomGMD_values geomGMD_io ${TINY_GRAPHS})
geomgmd_add_test(test_constrained_edge_map geomGMD_distance)

# Application test of geomGMD_matrix
add_executable(gurobi_check gurobi_check.cpp)
target_link_libraries(gurobi_check PRIVATE geomGMD_distance)
geomgmd_enable_warnings(gurobi_check)
configure_file(config_tiny_test.toml.in ${CMAKE_CURRENT_BINARY_DIR}/config_tiny_test.toml @ONLY)
add_test(NAME app_matrix
         COMMAND ${CMAKE_COMMAND}
                 -DGUROBI_CHECK=$<TARGET_FILE:gurobi_check>
                 -DAPP=$<TARGET_FILE:geomGMD_matrix>
                 -DCONFIG=${CMAKE_CURRENT_BINARY_DIR}/config_tiny_test.toml
                 -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/output/tiny/app_matrix
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/run_app_test.cmake)
set_tests_properties(app_matrix PROPERTIES SKIP_REGULAR_EXPRESSION "Test skipped" TIMEOUT 300)
