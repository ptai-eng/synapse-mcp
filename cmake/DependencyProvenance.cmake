# Shared fail-closed checks for owned header-only dependency checkouts.
cmake_minimum_required(VERSION 3.20)
find_package(Git REQUIRED QUIET)

function(synapse_verify_checkout source commit)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse HEAD
        RESULT_VARIABLE result OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT revision STREQUAL commit)
        message(FATAL_ERROR "Dependency checkout commit mismatch: ${source}")
    endif()
    # Skip-worktree/assume-unchanged flags must not hide dependency edits.
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" ls-files -v
        RESULT_VARIABLE result OUTPUT_VARIABLE indexed)
    if(NOT result EQUAL 0 OR indexed MATCHES "(^|\n)[^H] ")
        message(FATAL_ERROR "Dependency has unqualified index flags: ${source}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" diff --cached --quiet HEAD
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Dependency has staged edits: ${source}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" ls-files --others
        RESULT_VARIABLE result OUTPUT_VARIABLE untracked)
    if(NOT result EQUAL 0 OR NOT untracked STREQUAL "")
        message(FATAL_ERROR "Dependency has untracked content: ${source}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" -c core.autocrlf=false
        diff --no-ext-diff --name-only HEAD -- RESULT_VARIABLE result OUTPUT_VARIABLE changed
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect dependency changes")
    endif()
    string(REPLACE "\n" ";" changed "${changed}")
    foreach(path IN LISTS changed)
        if(NOT path IN_LIST ARGN)
            message(FATAL_ERROR "Dependency has unrelated edits: ${path}")
        endif()
    endforeach()
endfunction()

function(synapse_fetch_checkout source url commit)
    if(EXISTS "${source}/.git")
        return() # The caller must qualify this existing checkout, never reset it.
    endif()
    file(GLOB entries "${source}/*" "${source}/.*")
    if(entries)
        message(FATAL_ERROR "Pre-existing dependency source is unqualified: ${source}")
    endif()
    file(MAKE_DIRECTORY "${source}")
    foreach(arguments IN ITEMS "init" "config;core.autocrlf;false"
            "fetch;--depth=1;${url};${commit}" "checkout;--detach;FETCH_HEAD")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" ${arguments}
            RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Pinned dependency download failed: ${source}")
        endif()
    endforeach()
endfunction()
