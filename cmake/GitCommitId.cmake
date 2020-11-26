if(__feeds_git_commit_id_included)
    return()
endif()
set(__feeds_git_commit_id_included TRUE)

function(git_head_commit_id _commit_id)
    find_package(Git)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE HEAD_COMMIT_ID
        ERROR_VARIABLE ERROR_OUTPUT)
    if(NOT HEAD_COMMIT_ID)
        set(HEAD_COMMIT_ID "0000000")
    endif()
    string(SUBSTRING ${HEAD_COMMIT_ID} 0 7 HEAD_COMMIT_ID)
    set(${_commit_id} ${HEAD_COMMIT_ID} PARENT_SCOPE)
endfunction()
