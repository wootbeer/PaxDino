# Local fixes to the upstream submodules, kept as patch files in
# lib-patches/<submodule>/ and applied to the submodule's working tree at
# configure time. A stamp in the build folder records the patch set last
# applied; when the set changes (a patch added or edited), the submodule's
# tracked files are reset and the whole set is applied again in order, so a
# patch whose context a later one changes never has to be detected as
# already applied.
find_package(Git REQUIRED)

# trw_apply_lib_patches(<dir in lib-patches/> <path under the lib dir>)
function(trw_apply_lib_patches patch_dir submodule)
    file(GLOB patches "${CMAKE_SOURCE_DIR}/lib-patches/${patch_dir}/*.patch")
    list(SORT patches)
    set(signature "")
    foreach(patch IN LISTS patches)
        get_filename_component(name "${patch}" NAME)
        file(SHA1 "${patch}" hash)
        string(APPEND signature "${name} ${hash}\n")
    endforeach()

    set(tree "${TRW_LIB_DIR}/${submodule}")
    set(stamp "${CMAKE_BINARY_DIR}/lib-patches/${patch_dir}.stamp")
    if (EXISTS "${stamp}")
        file(READ "${stamp}" applied)
        if (applied STREQUAL signature)
            # Still applied unless the submodule was reset since.
            list(LENGTH patches count)
            if (count EQUAL 0)
                return()
            endif()
            list(GET patches -1 last)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace --reverse --check "${last}"
                WORKING_DIRECTORY "${tree}"
                RESULT_VARIABLE last_applied OUTPUT_QUIET ERROR_QUIET)
            if (last_applied EQUAL 0)
                return()
            endif()
        endif()
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" checkout -- .
        WORKING_DIRECTORY "${tree}"
        RESULT_VARIABLE result ERROR_VARIABLE error)
    if (NOT result EQUAL 0)
        message(FATAL_ERROR "Couldn't reset ${tree} before applying lib-patches/${patch_dir}:\n${error}")
    endif()
    # Files the patches add aren't tracked, so the checkout above leaves them;
    # remove them so the patch adding each one applies again.
    foreach(patch IN LISTS patches)
        file(READ "${patch}" patch_text)
        string(REGEX MATCHALL "--- /dev/null\r?\n\\+\\+\\+ b/[^\r\n]+" added "${patch_text}")
        foreach(entry IN LISTS added)
            string(REGEX REPLACE "^--- /dev/null\r?\n\\+\\+\\+ b/" "" added_path "${entry}")
            file(REMOVE "${tree}/${added_path}")
        endforeach()
    endforeach()
    foreach(patch IN LISTS patches)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${patch}"
            WORKING_DIRECTORY "${tree}"
            RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            message(FATAL_ERROR "Couldn't apply ${patch} to ${tree}:\n${error}")
        endif()
        message(STATUS "Applied ${patch} to ${tree}")
    endforeach()
    file(WRITE "${stamp}" "${signature}")
endfunction()
