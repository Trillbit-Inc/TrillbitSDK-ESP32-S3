set(COMPONENT_ADD_INCLUDEDIRS 
    "include"
    )

register_component()

#target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L ${CMAKE_CURRENT_SOURCE_DIR}/lib/GCC")
add_prebuilt_library(trill_core_sdk "${CMAKE_CURRENT_SOURCE_DIR}/lib/GCC/libtrill_core_sdk.a" PRIV_REQUIRES esp-dsp)
target_link_libraries(${COMPONENT_TARGET} INTERFACE "-Wl,--start-group"
            trill_core_sdk
            "-Wl,--end-group"
            )
