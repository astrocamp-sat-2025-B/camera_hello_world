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

uint8_t frame_buffer[FRAME_HEIGHT][FRAME_WIDTH];

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

void print_camera_format() {
    uint8_t com7_val, com15_val;
    
    // COM7レジスタの値を読み取る
    if (!camera_read_reg(REG_COM7, &com7_val)) {
        printf("COM7レジスタの読み取りに失敗しました。\n");
        return;
    }
    
    // COM15レジスタの値を読み取る
    if (!camera_read_reg(REG_COM15, &com15_val)) {
        printf("COM15レジスタの読み取りに失敗しました。\n");
        return;
    }

    printf("--- カメラ出力形式 ---\n");
    printf("レジスタ値: COM7=0x%02X, COM15=0x%02X\n", com7_val, com15_val);

    // COM7の値から基本フォーマットを判断
    // YUV, RGB, Bayer RAW, Processed Bayer RAW
    if ((com7_val & 0b00000101) == 0b00000000) { // Bit2=0, Bit0=0
        printf("基本形式: YUV\n");

        uint8_t tslb_val, com13_val;
        if (camera_read_reg(REG_TSLB, &tslb_val) && camera_read_reg(REG_COM13, &com13_val)) {
            bool tslb_bit3 = (tslb_val >> 3) & 1;
            bool com13_bit0 = com13_val & 1;

            if (!tslb_bit3 && !com13_bit0) {
                printf("バイト順: YUYV\n");
            } else if (!tslb_bit3 && com13_bit0) {
                printf("バイト順: YVYU\n");
            } else if (tslb_bit3 && !com13_bit0) {
                printf("バイト順: UYVY\n");
            } else { // tslb_bit3 && com13_bit0
                printf("バイト順: VYUY\n");
            }
        } else {
            printf("YUVの並び順を判定できませんでした。\n");
        }
    } else if ((com7_val & 0b00000101) == 0b00000100) { // Bit2=1, Bit0=0
        printf("基本形式: RGB\n");
        // RGBの場合、COM15で詳細を判断
        uint8_t rgb_format = (com15_val >> 4) & 0b11; // Bit5, Bit4
        if (rgb_format == 0b01) {
            printf("詳細形式: RGB565\n");
        } else if (rgb_format == 0b11) {
            printf("詳細形式: RGB555\n");
        } else {
            printf("詳細形式: 通常RGB\n");
        }
    } else if ((com7_val & 0b00000101) == 0b00000001) { // Bit2=0, Bit0=1
        printf("基本形式: Bayer RAW\n");
    } else { // 0b00000101 (Bit2=1, Bit0=1)
        printf("基本形式: Processed Bayer RAW\n");
    }

    // COM7の他のビットも解析
    if (com7_val & (1 << 3)) { // Bit3
        printf("解像度: QVGA\n");
    } else {
        printf("解像度: VGA (またはSCCBで設定された他の解像度)\n");
    }

    if (com7_val & (1 << 6)) { // Bit6
        printf("CCIR656形式: 有効\n");
    }
    printf("------------------------\n");
}

// カメラのレジスタに1バイト書き込む関数
bool camera_write_reg(uint8_t reg_addr, uint8_t data) {
    uint8_t buffer[2] = {reg_addr, data};
    if (i2c_write_blocking(I2C_PORT, CAMERA_ADDR, buffer, 2, false) != 2) {
        return false; // 書き込み失敗
    }
    return true;
}

// カメラの出力形式をRaw RGB (Bayer RAW) に設定する関数
void set_camera_format_raw_rgb() {
    printf("カメラの出力形式を Raw RGB (Bayer RAW) に設定します...\n");

    uint8_t com7_val;
    // まず現在のCOM7レジスタの値を読み取る
    if (!camera_read_reg(REG_COM7, &com7_val)) {
        printf("エラー: COM7の読み取りに失敗しました。\n");
        return;
    }

    // COM7レジスタのBit0を1に、Bit2を0に設定する
    // (他の設定ビットは変更しないようにビット演算を行う)
    com7_val |= (1 << 0);  // Bit0を1にセット (RAW)
    com7_val &= ~(1 << 2); // Bit2を0にクリア (RGBではなくRAW)

    // 変更した値をレジスタに書き戻す
    if (camera_write_reg(REG_COM7, com7_val)) {
        printf("COM7レジスタを 0x%02X に設定しました。\n", com7_val);
    } else {
        printf("エラー: COM7の書き込みに失敗しました。\n");
    }
}

void set_camera_format_rgb565() {
    uint8_t reg_val;

    // --- ステップ1: COM7レジスタを設定 (RGBモード) ---
    if (!camera_read_reg(REG_COM7, &reg_val)) { printf("エラー: COM7の読み取りに失敗。\n"); return; }
    reg_val |= (1 << 2);  // Bit2を1にセット (RGB)
    reg_val &= ~(1 << 0); // Bit0を0にクリア
    if (!camera_write_reg(REG_COM7, reg_val)) { printf("エラー: COM7の書き込みに失敗。\n"); return; }

    // --- ステップ2: COM15レジスタを設定 (RGB565モード) ---
    if (!camera_read_reg(REG_COM15, &reg_val)) { printf("エラー: COM15の読み取りに失敗。\n"); return; }
    reg_val |= (1 << 4);  // Bit4を1にセット (RGB565)
    reg_val &= ~(1 << 5); // Bit5を0にクリア
    if (!camera_write_reg(REG_COM15, reg_val)) { printf("エラー: COM15の書き込みに失敗。\n"); return; }

    // --- ステップ3: REG444レジスタを確認/設定 (RGB444を無効化) ---
    // RGB565を有効にするための前提条件
    if (!camera_read_reg(REG_REG444, &reg_val)) { printf("エラー: REG444の読み取りに失敗。\n"); return; }
    reg_val &= ~(1 << 1); // Bit1を0にクリア (RGB444無効)
    if (!camera_write_reg(REG_REG444, reg_val)) { printf("エラー: REG444の書き込みに失敗。\n"); return; }
}

// カメラの出力形式をYUV (YUYV) に設定する関数
void set_camera_format_yuv_yuyv() {
    uint8_t com7_val, tslb_val, com13_val;

    // --- ステップ1: COM7レジスタをYUVモードに設定 ---
    if (!camera_read_reg(REG_COM7, &com7_val)) {
        printf("エラー: COM7の読み取りに失敗。\n"); return;
    }
    // Bit2とBit0を両方0に設定
    com7_val &= ~((1 << 2) | (1 << 0));
    if (!camera_write_reg(REG_COM7, com7_val)) {
        printf("エラー: COM7の書き込みに失敗。\n"); return;
    }

    // --- ステップ2: TSLBレジスタで並び順を設定 ---
    if (!camera_read_reg(REG_TSLB, &tslb_val)) {
        printf("エラー: TSLBの読み取りに失敗。\n"); return;
    }
    // Bit[3]を0に設定 (YUYV or YVYU)
    tslb_val &= ~(1 << 3);
    if (!camera_write_reg(REG_TSLB, tslb_val)) {
        printf("エラー: TSLBの書き込みに失敗。\n"); return;
    }

    // --- ステップ3: COM13レジスタで並び順を設定 ---
    if (!camera_read_reg(REG_COM13, &com13_val)) {
        printf("エラー: COM13の読み取りに失敗。\n"); return;
    }
    // Bit[0] (UV swap) を0に設定 (YUYV)
    com13_val &= ~(1 << 0);
    if (!camera_write_reg(REG_COM13, com13_val)) {
        printf("エラー: COM13の書き込みに失敗。\n"); return;
    }
}

// PIOではなくGPIOピンからのデータキャプチャを行う関数（未実装）
void get_photo_frame(int width, int height) {

    // 1. フレーム開始待機 (VSYNCの立ち下がりを待つ)
    while(gpio_get(VSYNC_PIN)); // VSYNCがLOWになるのを待つ
    while(!gpio_get(VSYNC_PIN)); // VSYNCがHIGHになるのを待つ
    while(gpio_get(VSYNC_PIN)); // VSYNCがLOWになるのを待つ -> フレーム開始

    // 2. 指定された行数だけデータを読み取る
    for (int y = 0; y < height; y++) {
        // 3. 行の開始待機 (HREFの立ち上がりを待つ)
        while(!gpio_get(HREF_PIN));

        // 4. 指定されたピクセル数だけデータを読み取る
        for (int x = 0; x < width; x++) {
            // 5. ピクセルクロックの立ち上がりを待つ
            while(!gpio_get(PCLK_PIN));

            // 6. 8ビットのデータを読み取る
            uint8_t pixel_data = 0;
            // D7がMSBなので逆順に読み取る
            for (int i = 7; i >= 0; i--) {
                pixel_data |= (gpio_get(D0_PIN + i) << i);
            }
            frame_buffer[y][x] = pixel_data;

            // 7. ピクセルクロックの立ち下がりを待つ
            while(gpio_get(PCLK_PIN));
        }
        // 8. 行の終了を待つ (HREFの立ち下がり)
        while(gpio_get(HREF_PIN));
    }
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

    if (camera_read_reg(REG_PID, &pid) && camera_read_reg(REG_VER, &ver) &&
            pid == EXPECTED_PID && ver == EXPECTED_VER) {
        // set_camera_format_raw_rgb();
        set_camera_format_yuv_yuyv();
        // set_camera_format_rgb565();
        get_photo_frame(FRAME_WIDTH, FRAME_HEIGHT);
    } else {
        printf("I2Cエラー: カメラが見つかりません。\n");
    }

    int i = 0;
    while (true) {
        // if (camera_read_reg(REG_PID, &pid) && camera_read_reg(REG_VER, &ver) &&
        //     pid == EXPECTED_PID && ver == EXPECTED_VER) {

        //     get_photo_frame(FRAME_WIDTH, FRAME_HEIGHT);

            // // 取得したデータの最初の10バイトなどを表示して確認
            // printf("取得したフレームの左上10ピクセル:");
            // for(int i=0; i<10; i++) {
            //     printf(" %02X", frame_buffer[0][i]);
            // }
            // printf("\n");
            // 取得したデータを全て表示
            print_camera_format();
            sleep_ms(10000); // 10秒待機
            // printf("--- 取得したフレームデータ ---\n");
            for(int y=0; y<FRAME_HEIGHT; y++) {
                for(int x=0; x<FRAME_WIDTH; x++) {
                    printf("%02X ", frame_buffer[y][x]);
                }
                printf("\n");
            }
            // printf("--- フレーム取得完了 --- \n\n");
            sleep_ms(5000); // 5秒待機

        // } else {
        //     printf("I2Cエラー: カメラが見つかりません。\n");
        // }
    }
}
