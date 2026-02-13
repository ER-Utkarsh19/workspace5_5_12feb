idf_component_register(SRCS "dvp_spi_lcd_main.c" "model_inference.cc" "model_data.cc"
                    INCLUDE_DIRS "."
                    REQUIRES esp_mm esp_driver_cam esp_driver_i2c esp_lcd sensor_init spiffs esp-tflite-micro
                    )

# ADD THIS LINE TO IGNORE THE TFLITE WARNING:
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=deprecated-declarations)