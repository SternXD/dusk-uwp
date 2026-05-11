function(uwp_pkg TARGET SOURCE_DIR DEST_SUBDIR)
    file(GLOB_RECURSE FILES RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")

    foreach(FILE_IN_DIR ${FILES})
        # UWP needs dll's in the root, skip or it will overwrite main cmake config for special dll files
        if(FILE_IN_DIR MATCHES "\\.dll$")
            continue()
        endif()

        set(SRC "${SOURCE_DIR}/${FILE_IN_DIR}")

        get_filename_component(REL_DIR "${FILE_IN_DIR}" DIRECTORY)
        string(REPLACE "\\" "/" REL_DIR "${REL_DIR}")
        if(REL_DIR STREQUAL "" OR REL_DIR STREQUAL ".")
            set(DEPLOY_DIR "${DEST_SUBDIR}")
        else()
            set(DEPLOY_DIR "${DEST_SUBDIR}/${REL_DIR}")
        endif()

        set_source_files_properties("${SRC}" PROPERTIES
            VS_COPY_TO_OUT_DIR Always
            VS_DEPLOYMENT_CONTENT 1
            VS_DEPLOYMENT_LOCATION "${DEPLOY_DIR}"
        )

        target_sources(${TARGET} PRIVATE "${SRC}")
        source_group("Data" FILES "${SRC}")
    endforeach()
endfunction()
