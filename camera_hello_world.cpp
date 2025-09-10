#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "blink.pio.h"
#include "hardware/pwm.h"
// #include "camera.pio.h" 

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 15
#define I2C_BAUDRATE (100 * 1000) // 100kHz
#define XCLK_PIN 28

// カメラ
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
const uint8_t EXPECTED_PID = 0x76;
const uint8_t EXPECTED_VER = 0x73;


void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // 125MHzクロックを基準に、指定した周波数で点滅するように設定
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

void camera_xclk_init() {
    // GPIO28をPWM機能に設定
    gpio_set_function(XCLK_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(XCLK_PIN);
    uint chan = pwm_gpio_to_channel(XCLK_PIN);

    pwm_set_clkdiv(slice_num, 1.0f); // クロック分周を1に設定（125MHz）
    pwm_set_wrap(slice_num, 9);     // 10カウントで1周期（12.5MHz）

    // デューティ比を50%に設定
    pwm_set_chan_level(slice_num, chan, 5);

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
        return false; // レジスタアドレスの書き込み失敗
    }
    if (i2c_read_blocking(I2C_PORT, CAMERA_ADDR, data, 1, false) != 1) {
        return false; // データの読み取り失敗
    }
    return true;
}

void init_camera_pins() {
    // データピン D0-D7 (GPIO 0-7)
    for (int i = D0_PIN; i <= D7_PIN; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }
    // 同期ピン PCLK, HREF, VSYNC
    gpio_init(PCLK_PIN);
    gpio_set_dir(PCLK_PIN, GPIO_IN);

    gpio_init(HREF_PIN);
    gpio_set_dir(HREF_PIN, GPIO_IN);
    
    gpio_init(VSYNC_PIN);
    gpio_set_dir(VSYNC_PIN, GPIO_IN);
}

// PIOではなくGPIOピンからのデータキャプチャを行う関数（未実装）
void get_photo() {
}

int main()
{
    stdio_init_all();
    init_i2c();
    camera_xclk_init();
    init_camera_pins();

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

    uint8_t pid, ver;

    while (true) {
        bool success_pid = camera_read_reg(REG_PID, &pid);
        bool success_ver = camera_read_reg(REG_VER, &ver);

        if (success_pid && success_ver) {
            printf("製品ID (PID): 0x%02X, バージョン (VER): 0x%02X -> ", pid, ver);
            if (pid == EXPECTED_PID && ver == EXPECTED_VER) {
                printf("成功: OV7675を正常に検出しました。\n");
            } else {
                printf("失敗: 期待するIDと異なります。\n");
            }
        } else {
            printf("I2C通信エラー: カメラから応答がありません。\n");
        }

        // get_photo();
        sleep_ms(500);
    }
}
