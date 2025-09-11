#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "blink.pio.h"
#include "hardware/pwm.h"
// #include "camera.pio.h" 
#include "camera.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_BAUDRATE (100 * 1000) // 100kHz
#define XCLK_PIN 28

// カメラ
#define FRAME_WIDTH (320 * 2) // QVGAサイズ (320x240) を想定
// #define FRAME_WIDTH  320
#define FRAME_HEIGHT 240
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

// PIO
#define CAPTURE_PIO pio1
#define CAPTURE_SM 0

// I2C（SCCB）の7ビットスレーブアドレス, 8ビットアドレスでは0x42
const uint8_t CAMERA_ADDR = 0x42 >> 1;

// 製品IDのレジスタアドレス
const uint8_t REG_PID = 0x0A;
const uint8_t REG_VER = 0x0B;
const uint8_t REG_COM7 = 0x12;
const uint8_t REG_COM15 = 0x40;
const uint8_t REG_REG444 = 0x8C;
const uint8_t REG_TSLB = 0x3A;
const uint8_t REG_COM13 = 0x3D;
const uint8_t EXPECTED_PID = 0x76;
const uint8_t EXPECTED_VER = 0x73;

static uint8_t frame_buffer[FRAME_HEIGHT * FRAME_WIDTH];

void set_camera_format_yuv_yuyv();

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // 125MHzクロックを基準に、指定した周波数で点滅するように設定
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

int main()
{
    stdio_init_all();
    init_i2c();
    init_camera_settings();

    PIO pio;
    uint sm, offset;
    // uint offset = pio_add_program(pio, &blink_program); printf("Loaded program at %d\n", offset);
    if (pio_claim_free_sm_and_add_program(&blink_program, &pio, &sm, &offset)) {
        printf("PIO %d SM %d offset %d\n", pio == pio0 ? 0 : 1, sm, offset);
        #ifdef PICO_DEFAULT_LED_PIN
            blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
        #else
            blink_pin_forever(pio, 0, offset, 6, 3);
        #endif
    } else {
        printf("No PIO SMs available\n");
        return 1;
    }

    // get_photo_frame(frame_buffer, FRAME_WIDTH, FRAME_HEIGHT);
    capture_and_send_frame(CameraOutputFormat::YUV_YUYV, frame_buffer, FRAME_WIDTH, FRAME_HEIGHT);

    while (true) {
        // 取得したデータを全て表示
        print_camera_format();
        // printf("--- 取得したフレームデータ ---\n");
        sleep_ms(5000); // 5秒待機
        for(int y=0; y<FRAME_HEIGHT; y++) {
            for(int x=0; x<FRAME_WIDTH; x++) {
                printf("%02X ", frame_buffer[y * FRAME_WIDTH + x]);
            }
            printf("\n");
        }
        // printf("--- フレーム取得完了 --- \n\n");
        sleep_ms(5000); // 5秒待機
    }
}
