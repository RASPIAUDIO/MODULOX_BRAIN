idf_component_register(
    SRCS "main.cpp" "audio_output.cpp" "synth_voice.cpp" "osc.cpp" "adsr.cpp"
         "display.cpp" "controls.cpp" "filter.cpp" "patch_store.cpp" "patch_usb.cpp" "waves.cpp"
         "runtime_diagnostics.cpp" "sample_store.cpp" "firmware_update.cpp"
    INCLUDE_DIRS "." "${CMAKE_BINARY_DIR}/generated"
    PRIV_REQUIRES esp_driver_i2c esp_driver_i2s esp_driver_spi esp_driver_gpio esp_driver_uart esp_lcd spiffs json nvs_flash mbedtls esp_partition esp_timer app_update bootloader_support esp_psram spi_flash
    LDFRAGMENTS "dsp.lf"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
option(MODULOX_ROLLBACK_TEST "Build a test image which intentionally does not confirm its first boot" OFF)
option(MODULOX_USB_DIAGNOSTICS "Record startup USB controller registers in diagnostic NVS keys" OFF)
if(MODULOX_USB_DIAGNOSTICS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MODULOX_USB_DIAGNOSTICS=1)
endif()
if(MODULOX_ROLLBACK_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MODULOX_ROLLBACK_TEST=1)
endif()
# Collect compiler garbage more often on memory-constrained Windows build hosts.
# This changes compiler memory use, not optimization of the generated audio code.
if(CMAKE_HOST_WIN32)
    target_compile_options(${COMPONENT_LIB} PRIVATE --param=ggc-min-expand=10 --param=ggc-min-heapsize=16384)
endif()

spiffs_create_partition_image(patches "${CMAKE_CURRENT_SOURCE_DIR}/../patches" FLASH_IN_PROJECT)
