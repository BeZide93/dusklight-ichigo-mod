execute_process(COMMAND "${NM}" -u "${MOD_FILE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not inspect mod imports: ${error}")
endif()
string(REGEX MATCHALL "[ \t]svc_[A-Za-z0-9_]+" missing_services "${symbols}")
if(missing_services)
    message(FATAL_ERROR "Missing IMPORT_SERVICE definitions: ${missing_services}")
endif()
