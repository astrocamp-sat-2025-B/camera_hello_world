// Camera-related interface extracted from camera_hello_world.cpp
#pragma once

#include <stdint.h>

enum class CameraOutputFormat {
    YUV_YUYV,
    RGB565,
    RAW_BAYER
};

// Initialization
void init_camera_settings(void);
void init_i2c(void);
void camera_xclk_init(void);
void init_camera_pins(void);

// Register access
bool camera_read_reg(uint8_t reg_addr, uint8_t *data);
bool camera_write_reg(uint8_t reg_addr, uint8_t data);

// Formats and diagnostics
void set_camera_format_yuv_yuyv(void);
void set_camera_format_rgb565(void);
void set_camera_format_raw_rgb(void);
void print_camera_format(void);

// Frame capture (GPIO polling version)
void capture_and_send_frame(CameraOutputFormat fmt, uint8_t* buffer, int width, int height);
void get_photo_frame(uint8_t *buffer, int width, int height);
