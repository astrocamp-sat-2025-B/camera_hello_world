# import numpy as np
# from PIL import Image

# # --- 設定項目 ---
# # 16進数データが含まれるファイルのパス
# FILE_PATH = "hex.log"

# # 生成する画像の幅と高さ (VGAサイズをデフォルトに設定)
# # この値が正しくないと画像が乱れます。QVGA(320, 240)なども試してみてください。
# WIDTH = 320
# HEIGHT = 280

# # 出力する画像ファイル名
# OUTPUT_PATH = "output_image.png"
# # --- 設定ここまで ---


# def convert_yuyv_to_image(file_path, width, height):
#     """
#     16進数のテキストファイルを読み込み、YUYV422画像に変換する関数
#     """
#     try:
#         # 1. 16進数データの読み込みと解析
#         with open(file_path, "r") as f:
#             # ファイルを読み込み、スペースや改行で分割してリスト化
#             hex_data = f.read().split()

#         # 16進文字列を整数（バイト）のリストに変換
#         byte_data = [int(h, 16) for h in hex_data]

#         # 必要なデータ量を確認
#         expected_bytes = width * height * 2
#         if len(byte_data) < expected_bytes:
#             print(f"エラー: データが不足しています。")
#             print(f"必要なバイト数: {expected_bytes}, 実際のバイト数: {len(byte_data)}")
#             # 足りない分を0で埋める（画像の一部が黒くなる）
#             byte_data.extend([0] * (expected_bytes - len(byte_data)))

#         # NumPy配列に変換
#         yuyv_array = np.array(byte_data[:expected_bytes], dtype=np.uint8)

#         # 2. YUYVデータからY, U, V成分の分離
#         # 1次元配列を (高さ, 幅*2) の2次元配列に変換
#         # YUYVは2ピクセルで4バイトなので、1ピクセルあたり平均2バイト
#         yuyv_array = yuyv_array.reshape((height, width * 2))

#         # Y (輝度) 成分を抽出 (0, 2, 4, ...列目)
#         y = yuyv_array[:, 0::2]

#         # U (色差) 成分を抽出 (1, 5, 9, ...列目)
#         u = yuyv_array[:, 1::4]
#         # V (色差) 成分を抽出 (3, 7, 11, ...列目)
#         v = yuyv_array[:, 3::4]

#         # UとVは2ピクセルで1つなので、各ピクセルで使えるように配列を複製・拡張
#         u = np.repeat(u, 2, axis=1)
#         v = np.repeat(v, 2, axis=1)

#         # 3. YUVからRGBへの色空間変換
#         # 計算を浮動小数点で行うために型を変換
#         y = y.astype(float)
#         u = u.astype(float) - 128.0
#         v = v.astype(float) - 128.0

#         # YUVからRGBへの標準的な変換式を適用
#         r = y + 1.402 * v
#         g = y - 0.344136 * u - 0.714136 * v
#         b = y + 1.772 * u

#         # 0-255の範囲に収まるように値をクリップ
#         r = np.clip(r, 0, 255)
#         g = np.clip(g, 0, 255)
#         b = np.clip(b, 0, 255)

#         # RGBデータを結合して (高さ, 幅, 3) の配列を作成
#         rgb_array = np.stack((r, g, b), axis=-1).astype(np.uint8)

#         # 4. 画像の生成と保存
#         img = Image.fromarray(rgb_array, "RGB")
#         img.save(OUTPUT_PATH)
#         print(f"画像が '{OUTPUT_PATH}' として正常に保存されました。")
#         # img.show() # 画像を画面に表示したい場合はこの行のコメントを外す

#     except FileNotFoundError:
#         print(f"エラー: ファイル '{file_path}' が見つかりません。")
#     except Exception as e:
#         print(f"エラーが発生しました: {e}")


# if __name__ == "__main__":
#     convert_yuyv_to_image(FILE_PATH, WIDTH, HEIGHT)

import os
import re
import numpy as np
from PIL import Image

# --- 設定項目 ---
# 16進数データが含まれるファイルのパス
FILE_PATH = "screenlog.0"

# 生成する画像の幅と高さ (QVGAサイズを推奨)
# この値が正しくないと画像が乱れます。VGA(640, 480)なども試してみてください。
WIDTH = 320
HEIGHT = 240

# 出力ファイルの接頭語と自動採番用のパターン
FILENAME_PREFIX = "output_grayscale_image"
FILENAME_REGEX = re.compile(r"^" + re.escape(FILENAME_PREFIX) + r"(\\d+)\\.png$")
# --- 設定ここまで ---


def _next_output_path(base_dir: str = ".") -> str:
    """同名パターンの既存ファイルを確認し、次の連番ファイルパスを返す。

    形式: output_grayscale_image<num>.png （num は既存最大+1、存在しなければ 1）
    """
    max_num = 0
    try:
        for name in os.listdir(base_dir):
            m = FILENAME_REGEX.match(name)
            if m:
                try:
                    num = int(m.group(1))
                    if num > max_num:
                        max_num = num
                except ValueError:
                    # 数値化できない場合は無視
                    pass
    except FileNotFoundError:
        # ディレクトリがない場合は作成を試みる
        os.makedirs(base_dir, exist_ok=True)
        max_num = 0

    next_num = max_num + 1
    return os.path.join(base_dir, f"{FILENAME_PREFIX}{next_num}.png")


def convert_y_to_grayscale_image(file_path, width, height):
    """
    16進数のYUYV422データからY成分のみを抽出し、白黒画像を生成する関数
    """
    try:
        # 1. 16進数データの読み込みと解析
        with open(file_path, "r") as f:
            hex_data = f.read().split()

        byte_data = [int(h, 16) for h in hex_data]

        expected_bytes = width * height * 2
        if len(byte_data) < expected_bytes:
            print(f"警告: データが不足しています。")
            print(f"必要なバイト数: {expected_bytes}, 実際のバイト数: {len(byte_data)}")
            byte_data.extend([0] * (expected_bytes - len(byte_data)))

        yuyv_array = np.array(byte_data[:expected_bytes], dtype=np.uint8)

        # 2. YUYVデータからY成分のみを分離
        # 1次元配列を (高さ, 幅*2) の2次元配列に変換
        yuyv_array = yuyv_array.reshape((height, width * 2))

        # Y (輝度) 成分を抽出 (0, 2, 4, ...列目)
        # これがそのまま白黒画像のピクセルデータになる
        y_array = yuyv_array[:, 0::2]

        # 3. 画像の生成と保存
        # Y成分の2次元配列を元に、'L' (Luminance) モードで白黒画像を生成
        img = Image.fromarray(y_array, "L")

        # 保存先パスを自動採番
        save_path = _next_output_path(".")
        img.save(save_path)
        print(f"白黒画像が '{save_path}' として正常に保存されました。")

    except FileNotFoundError:
        print(f"エラー: ファイル '{file_path}' が見つかりません。")
    except ValueError as e:
        print(
            f"データ形式のエラー: 16進数でない値が含まれている可能性があります。 - {e}"
        )
    except Exception as e:
        print(f"エラーが発生しました: {e}")


if __name__ == "__main__":
    convert_y_to_grayscale_image(FILE_PATH, WIDTH, HEIGHT)
