#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "blink.pio.h"
#include "hardware/pwm.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define XCLK_PIN 28

// I2C（SCCB）の7ビットスレーブアドレス, 8ビットアドレスでは0x42
const uint8_t CAMERA_ADDR = 0x42 >> 1;

// 製品IDのレジスタアドレス
const uint8_t REG_PID = 0x0A;
const uint8_t REG_VER = 0x0B;


void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
}

void camera_xclk_init() {
    // GPIO28をPWM機能に設定
    gpio_set_function(XCLK_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(XCLK_PIN);

    pwm_set_clkdiv(slice_num, 1.0f); // 一旦停止
    pwm_set_wrap(slice_num, 9);     // 10サイクルで1周期

    // デューティ比を50%に設定
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 5);

    pwm_set_enabled(slice_num, true);
}


int main()
{
    stdio_init_all();
    camera_xclk_init();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    camera_xclk_init(); // XCLKの初期化
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // PIO Blinking example
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);
    
    #ifdef PICO_DEFAULT_LED_PIN
    blink_pin_forever(pio, 0, offset, PICO_DEFAULT_LED_PIN, 3);
    #else
    blink_pin_forever(pio, 0, offset, 6, 3);
    #endif
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio

    uint8_t pid_val, ver_val;

    while (true) {
        // --- PID (0x0A) の読み取り ---
        // まず、読み取りたいレジスタのアドレスをカメラに書き込む
        int ret1 = i2c_write_blocking(i2c1, CAMERA_ADDR, &REG_PID, 1, true);
        // 次に、カメラから1バイトのデータを読み取る
        int ret2 = i2c_read_blocking(i2c1, CAMERA_ADDR, &pid_val, 1, false);
        // ret1とret2で通信の成否を確認できる。0以上なら成功、負なら失敗
        printf("I2C書き込み結果=%d, 読み取り結果=%d\n", ret1, ret2);
        if (ret1 == PICO_ERROR_GENERIC || ret2 == PICO_ERROR_GENERIC) {
            printf("I2C通信エラーです。\n");
            sleep_ms(10);
            continue;
        }

        // --- VER (0x0B) の読み取り ---
        i2c_write_blocking(i2c1, CAMERA_ADDR, &REG_VER, 1, true);
        i2c_read_blocking(i2c1, CAMERA_ADDR, &ver_val, 1, false);

        printf("製品ID (PID): 0x%02X\n", pid_val);
        printf("バージョン (VER): 0x%02X\n", ver_val);

        if (pid_val == 0x76 && ver_val == 0x73) {
            printf("成功: OV7675を正常に検出しました。\n");
        } else {
            printf("失敗: カメラが検出できませんでした。配線やアドレスを確認してください。\n");
        }
        sleep_ms(1000);
    }
}
