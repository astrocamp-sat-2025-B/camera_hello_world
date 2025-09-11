// Camera-related implementation extracted from camera_hello_world.cpp

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

#include "camera.h"

// I2C setup
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_BAUDRATE (100 * 1000) // 100kHz

// XCLK output pin (PWM)
#define XCLK_PIN 28

// Camera data and sync pins
#define D0_PIN 0
#define D1_PIN 1
#define D2_PIN 2
#define D3_PIN 3
#define D4_PIN 4
#define D5_PIN 5
#define D6_PIN 6
#define D7_PIN 7
#define PCLK_PIN 22
#define HREF_PIN 26
#define VSYNC_PIN 27

// I2C（SCCB）の7ビットスレーブアドレス, 8ビットアドレスでは0x42
static const uint8_t CAMERA_ADDR = 0x42 >> 1;

// Registers
static const uint8_t REG_PID = 0x0A;
static const uint8_t REG_VER = 0x0B;
static const uint8_t REG_COM7 = 0x12;
static const uint8_t REG_COM15 = 0x40;
static const uint8_t REG_REG444 = 0x8C;
static const uint8_t REG_TSLB = 0x3A;
static const uint8_t REG_COM13 = 0x3D;
static const uint8_t EXPECTED_PID = 0x76;
static const uint8_t EXPECTED_VER = 0x73;

void init_camera_settings() {
    camera_xclk_init();
    init_camera_pins();
}

void camera_xclk_init() {
    gpio_set_function(XCLK_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(XCLK_PIN);
    uint chan = pwm_gpio_to_channel(XCLK_PIN);

    pwm_set_clkdiv(slice_num, 1.0f); // 125MHz
    pwm_set_wrap(slice_num, 9);       // 12.5MHz

    pwm_set_chan_level(slice_num, chan, 5); // 50%
    pwm_set_enabled(slice_num, true);
}

void init_i2c() {
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

bool camera_read_reg(uint8_t reg_addr, uint8_t *data) {
    if (i2c_write_blocking(I2C_PORT, CAMERA_ADDR, &reg_addr, 1, true) != 1) {
        return false;
    }
    if (i2c_read_blocking(I2C_PORT, CAMERA_ADDR, data, 1, false) != 1) {
        return false;
    }
    return true;
}

bool camera_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t buffer[2] = {reg_addr, data};
    return i2c_write_blocking(I2C_PORT, CAMERA_ADDR, buffer, 2, false) == 2;
}

void init_camera_pins() {
    // Data pins
    for (int i = D0_PIN; i <= D7_PIN; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }
    // Sync pins
    gpio_init(PCLK_PIN); gpio_set_dir(PCLK_PIN, GPIO_IN);
    gpio_init(HREF_PIN); gpio_set_dir(HREF_PIN, GPIO_IN);
    gpio_init(VSYNC_PIN); gpio_set_dir(VSYNC_PIN, GPIO_IN);

    uint8_t pid, ver;
    if (camera_read_reg(REG_PID, &pid) && camera_read_reg(REG_VER, &ver) &&
        pid == EXPECTED_PID && ver == EXPECTED_VER) {
        set_camera_format_yuv_yuyv();
    } else {
        printf("I2Cエラー: カメラが見つかりません。\n");
    }
}

void print_camera_format() {
    uint8_t com7_val, com15_val;
    if (!camera_read_reg(REG_COM7, &com7_val)) {
        printf("COM7レジスタの読み取りに失敗しました。\n");
        return;
    }
    if (!camera_read_reg(REG_COM15, &com15_val)) {
        printf("COM15レジスタの読み取りに失敗しました。\n");
        return;
    }

    printf("--- カメラ出力形式 ---\n");
    printf("レジスタ値: COM7=0x%02X, COM15=0x%02X\n", com7_val, com15_val);

    if ((com7_val & 0b00000101) == 0b00000000) {
        printf("基本形式: YUV\n");
        uint8_t tslb_val, com13_val;
        if (camera_read_reg(REG_TSLB, &tslb_val) && camera_read_reg(REG_COM13, &com13_val)) {
            bool tslb_bit3 = (tslb_val >> 3) & 1;
            bool com13_bit0 = com13_val & 1;
            if (!tslb_bit3 && !com13_bit0) printf("バイト順: YUYV\n");
            else if (!tslb_bit3 && com13_bit0) printf("バイト順: YVYU\n");
            else if (tslb_bit3 && !com13_bit0) printf("バイト順: UYVY\n");
            else printf("バイト順: VYUY\n");
        } else {
            printf("YUVの並び順を判定できませんでした。\n");
        }
    } else if ((com7_val & 0b00000101) == 0b00000100) {
        printf("基本形式: RGB\n");
        uint8_t rgb_format = (com15_val >> 4) & 0b11; // Bit5, Bit4
        if (rgb_format == 0b01) printf("詳細形式: RGB565\n");
        else if (rgb_format == 0b11) printf("詳細形式: RGB555\n");
        else printf("詳細形式: 通常RGB\n");
    } else if ((com7_val & 0b00000101) == 0b00000001) {
        printf("基本形式: Bayer RAW\n");
    } else {
        printf("基本形式: Processed Bayer RAW\n");
    }

    if (com7_val & (1 << 3)) printf("解像度: QVGA\n");
    else printf("解像度: VGA (またはSCCBで設定された他の解像度)\n");
    if (com7_val & (1 << 6)) printf("CCIR656形式: 有効\n");
    printf("------------------------\n");
}

void set_camera_format_raw_rgb() {
    printf("カメラの出力形式を Raw RGB (Bayer RAW) に設定します...\n");
    uint8_t com7_val;
    if (!camera_read_reg(REG_COM7, &com7_val)) { printf("エラー: COM7の読み取りに失敗しました。\n"); return; }
    com7_val |= (1 << 0);  // RAW
    com7_val &= ~(1 << 2); // RGBビットをクリア
    if (!camera_write_reg(REG_COM7, com7_val)) { printf("エラー: COM7の書き込みに失敗しました。\n"); }
}

void set_camera_format_rgb565() {
    uint8_t reg_val;
    if (!camera_read_reg(REG_COM7, &reg_val)) { printf("エラー: COM7の読み取りに失敗。\n"); return; }
    reg_val |= (1 << 2);  // RGB
    reg_val &= ~(1 << 0);
    if (!camera_write_reg(REG_COM7, reg_val)) { printf("エラー: COM7の書き込みに失敗。\n"); return; }

    if (!camera_read_reg(REG_COM15, &reg_val)) { printf("エラー: COM15の読み取りに失敗。\n"); return; }
    reg_val |= (1 << 4);  // RGB565
    reg_val &= ~(1 << 5);
    if (!camera_write_reg(REG_COM15, reg_val)) { printf("エラー: COM15の書き込みに失敗。\n"); return; }

    if (!camera_read_reg(REG_REG444, &reg_val)) { printf("エラー: REG444の読み取りに失敗。\n"); return; }
    reg_val &= ~(1 << 1); // RGB444無効
    if (!camera_write_reg(REG_REG444, reg_val)) { printf("エラー: REG444の書き込みに失敗。\n"); return; }
}

void set_camera_format_yuv_yuyv() {
    uint8_t com7_val, tslb_val, com13_val;
    if (!camera_read_reg(REG_COM7, &com7_val)) { printf("エラー: COM7の読み取りに失敗。\n"); return; }
    com7_val &= ~((1 << 2) | (1 << 0));
    if (!camera_write_reg(REG_COM7, com7_val)) { printf("エラー: COM7の書き込みに失敗。\n"); return; }

    if (!camera_read_reg(REG_TSLB, &tslb_val)) { printf("エラー: TSLBの読み取りに失敗。\n"); return; }
    tslb_val &= ~(1 << 3);
    if (!camera_write_reg(REG_TSLB, tslb_val)) { printf("エラー: TSLBの書き込みに失敗。\n"); return; }

    if (!camera_read_reg(REG_COM13, &com13_val)) { printf("エラー: COM13の読み取りに失敗。\n"); return; }
    com13_val &= ~(1 << 0); // YUYV
    if (!camera_write_reg(REG_COM13, com13_val)) { printf("エラー: COM13の書き込みに失敗。\n"); return; }
}

void get_photo_frame(uint8_t *buffer, int width, int height) {
    // 1. Frame start wait (VSYNC falling -> rising -> falling)
    while(gpio_get(VSYNC_PIN));
    while(!gpio_get(VSYNC_PIN));
    while(gpio_get(VSYNC_PIN));

    for (int y = 0; y < height; y++) {
        while(!gpio_get(HREF_PIN));
        for (int x = 0; x < width; x++) {
            while(!gpio_get(PCLK_PIN));
            uint8_t pixel_data = 0;
            for (int i = 7; i >= 0; i--) {
                pixel_data |= (gpio_get(D0_PIN + i) << i);
            }
            buffer[y * width + x] = pixel_data;
            while(gpio_get(PCLK_PIN));
        }
        while(gpio_get(HREF_PIN));
    }
}

static void set_format(CameraOutputFormat fmt) {
    switch (fmt) {
        case CameraOutputFormat::YUV_YUYV:
            set_camera_format_yuv_yuyv();
            break;
        case CameraOutputFormat::RGB565:
            set_camera_format_rgb565();
            break;
        case CameraOutputFormat::RAW_BAYER:
            set_camera_format_raw_rgb();
            break;
        default:
            // Fallback to YUYV
            set_camera_format_yuv_yuyv();
            break;
    }
}

void capture_and_send_frame(CameraOutputFormat fmt, uint8_t* buffer, int width, int height) {
    set_format(fmt);
    get_photo_frame(buffer, width, height);
    // WiFiで送信するモック関数
    // send_frame_wireless(buffer, width, height);
}
