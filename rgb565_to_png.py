import numpy as np
from PIL import Image

# --- 設定項目 ---
# 16進数データが含まれるファイルのパス
FILE_PATH = "screenlog.0"

# 生成する画像の幅と高さ
WIDTH = 320
HEIGHT = 240

# 出力する画像ファイル名
OUTPUT_PATH = "output_rgb565_image.png"
# --- 設定ここまで ---


def convert_rgb565_to_image(file_path, width, height):
    """
    16進数のバイナリファイルを読み込み、RGB565フォーマットの画像に変換する関数
    """
    try:
        # 1. 16進数データの読み込みと解析
        with open(file_path, "r") as f:
            # ファイルを読み込み、スペースや改行で分割してリスト化
            hex_data = f.read().split()

        # 16進文字列を整数（バイト）のリストに変換
        byte_data = [int(h, 16) for h in hex_data]

        # 必要なデータ量を確認 (1ピクセル = 2バイト)
        expected_bytes = width * height * 2
        if len(byte_data) < expected_bytes:
            print(f"警告: データが不足しています。")
            print(f"必要なバイト数: {expected_bytes}, 実際のバイト数: {len(byte_data)}")
            # 足りない分を0で埋める
            byte_data.extend([0] * (expected_bytes - len(byte_data)))

        # NumPy配列に変換 (データ型は符号なし8ビット整数)
        raw_array = np.array(byte_data[:expected_bytes], dtype=np.uint8)

        # 2. RGB565データのデコード
        # 1次元のバイト配列を (高さ, 幅, 2バイト/ピクセル) の3次元配列に変換
        # これで各ピクセルが2つのバイトを持つことになる
        pixel_bytes = raw_array.reshape((height, width, 2))

        # 2つの8ビットバイトを1つの16ビット整数に結合
        # ここではバイトオーダーをビッグエンディアン (最初のバイトが上位) と仮定
        # byte1 << 8 | byte2
        rgb565_array = (pixel_bytes[:, :, 0].astype(np.uint16) << 8) | (
            pixel_bytes[:, :, 1].astype(np.uint16)
        )

        # 3. 16ビットデータからR, G, B成分を抽出
        # R (赤): 上位5ビット
        # G (緑): 中位6ビット
        # B (青): 下位5ビット
        #   15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
        # | R  R  R  R  R | G  G  G  G  G  G | B  B  B  B  B |

        r_5bit = (rgb565_array >> 11) & 0b11111
        g_6bit = (rgb565_array >> 5) & 0b111111
        b_5bit = rgb565_array & 0b11111

        # 4. 各成分を8ビット (0-255) のレンジに変換
        # 例: 5ビット(0-31) -> 8ビット(0-255) は、値を 255/31 倍する
        r_8bit = (r_5bit * 255 / 31).astype(np.uint8)
        g_8bit = (g_6bit * 255 / 63).astype(np.uint8)
        b_8bit = (b_5bit * 255 / 31).astype(np.uint8)

        # 5. R, G, Bの配列を結合して最終的な画像配列を作成
        # (高さ, 幅, 3色) の構造にする
        rgb_array = np.stack((r_8bit, g_8bit, b_8bit), axis=-1)

        # 6. 画像の生成と保存
        img = Image.fromarray(rgb_array, "RGB")
        img.save(OUTPUT_PATH)
        print(f"RGB565画像が '{OUTPUT_PATH}' として正常に保存されました。")

    except FileNotFoundError:
        print(f"エラー: ファイル '{file_path}' が見つかりません。")
    except Exception as e:
        print(f"エラーが発生しました: {e}")


if __name__ == "__main__":
    convert_rgb565_to_image(FILE_PATH, WIDTH, HEIGHT)
