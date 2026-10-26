# Application test: runs geomGMD_matrix on the tiny example dataset and checks that it succeeds and writes
# its result files. Skipped (message "Test skipped", see SKIP_REGULAR_EXPRESSION) without a Gurobi licence.
#
# Variables: GUROBI_CHECK, APP, CONFIG, OUTPUT_DIR (run folder)

execute_process(COMMAND "${GUROBI_CHECK}" RESULT_VARIABLE gurobi_result)
if(NOT gurobi_result EQUAL 0)
  message("Test skipped: Gurobi is not available")
  return()
endif()

execute_process(COMMAND "${APP}" "${CONFIG}" --overwrite RESULT_VARIABLE app_result)
if(NOT app_result EQUAL 0)
  message(FATAL_ERROR "${APP} failed with exit code ${app_result}")
endif()

foreach(file intervaldistance_matrix.csv computation_info.csv edgemaps.json run.log config_input.toml)
  if(NOT EXISTS "${OUTPUT_DIR}/${file}")
    message(FATAL_ERROR "${APP} did not write ${OUTPUT_DIR}/${file}")
  endif()
endforeach()

# 6 pairs, all optimal
file(STRINGS "${OUTPUT_DIR}/intervaldistance_matrix.csv" rows REGEX ",true$")
list(LENGTH rows num_optimal)
if(NOT num_optimal EQUAL 6)
  message(FATAL_ERROR "expected 6 optimal pairs in intervaldistance_matrix.csv, found ${num_optimal}")
endif()
