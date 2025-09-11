import numpy as np
from PIL import Image

# --- 設定項目 ---
# 16進数データが含まれるファイルのパス
FILE_PATH = "papercop"
# FILE_PATH = "screenlog.0"

# 生成する画像の幅と高さ (QVGAサイズを推奨)
# WIDTH = 160
# HEIGHT = 240
WIDTH = 160
HEIGHT = 480

# 出力する画像ファイル名
OUTPUT_PATH = "output_grayscale_image_line_by_line.png"
# --- 設定ここまで ---


def convert_y_line_by_line(file_path, width, height):
    """
    16進数データを1行ずつ読み込み、Y成分を抽出しながら
    二次元配列を構築して白黒画像を生成する関数
    """
    try:
        # 1. 完成した画像データを格納するための空のリストを準備
        image_data_rows = []

        # 2. ファイルを1行ずつ処理する
        with open(file_path, "r") as f:
            for i, line in enumerate(f):
                # heightで指定された行数に達したら処理を終了
                if i >= height:
                    break

                # 3. 1行のデータを処理
                # 行をスペースで区切り、16進数の文字列リストにする
                hex_values_in_line = line.split()

                print(f"行 {i+1}: 元のデータ数 {len(hex_values_in_line)}")

                # YUYVデータなので、1行には width * 2 個のデータがあるはず
                # そこからY成分だけを1つおきに抜き出す
                # y_hex_values = hex_values_in_line[0::2]

                # 4. 1行分のY成分を10進数に変換し、リストとして作成
                # row_pixels = [int(h, 16) for h in y_hex_values]
                row_pixels = [int(h, 16) for h in hex_values_in_line[:width]]

                print(f"行 {i+1}: Y成分を {len(row_pixels)} 個抽出しました。")

                # 5. 完成した1行分のピクセルデータを全体のリストに追加
                image_data_rows.append(row_pixels)

        # 6. NumPy配列への変換
        # PythonのリストのリストをNumPyの二次元配列に変換
        # データ型を画像に適した 'uint8' (0-255) に指定
        y_array = np.array(image_data_rows, dtype=np.uint8)

        # 7. 画像の生成と保存
        img = Image.fromarray(y_array, "L")
        img.save(OUTPUT_PATH)
        print(f"白黒画像が '{OUTPUT_PATH}' として正常に保存されました。")

    except FileNotFoundError:
        print(f"エラー: ファイル '{file_path}' が見つかりません。")
    except Exception as e:
        print(f"エラーが発生しました: {e}")


if __name__ == "__main__":
    convert_y_line_by_line(FILE_PATH, WIDTH, HEIGHT)
