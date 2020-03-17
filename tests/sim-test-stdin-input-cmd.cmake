message("Test command: cat ${INPUT_FILE} | ${PASIM} -V --maxc 40000 ${TEST_BIN}")
execute_process(
	COMMAND cat ${INPUT_FILE}
	COMMAND ${PASIM} -V --maxc 40000 ${TEST_BIN}  
	OUTPUT_VARIABLE OUT ERROR_VARIABLE OUT 
	RESULT_VARIABLE CMD_RESULT
)
message("${OUT}")
if(CMD_RESULT)
	message(FATAL_ERROR "Error running test")
endif()