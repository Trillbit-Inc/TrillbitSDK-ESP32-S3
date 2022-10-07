set(COMPONENT_SRCS
    main.c
    storage.c
    ui.c
    serial_com.c
    )

set(COMPONENT_ADD_INCLUDEDIRS 
    include
    )

set(COMPONENT_REQUIRES
    bsp
    mtfsk_trill_sdk
    esp-dsp
    lvgl
    console
    )

register_component()

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)



