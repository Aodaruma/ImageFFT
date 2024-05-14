# ImageFFT

画像のフーリエ変換・逆フーリエ変換を行うプラグインです。

![ImageFFT](https://github.com/Aodaruma/ImageFFT/blob/main/thumnbnail.png?raw=true)

現在は **version 1.0** が最新です。

## FFT@ImageFFT

以下はパラメーターの説明です。

-   **トラックバー・チェックボックス**
    -   Channel: FFT を実行する画像チャンネル [All, Mono, Red, Green, Blue, Alpha]
    -   Re<->Im: 出力する画像の実部・虚部切り替え（Channel == "All"のみ有効; その他では R で実部・G で虚部を出力します。）
    -   SwapQ: 周波数成分の象限入れ替えを行うかどうか
    -   OutCurve: 出力値に対する変換の種類 [Linear, Log2]
    -   Enable: FFT を実行するかどうか
    -   ShowHelp: ヘルプを表示するかどうか

## IFFT@ImageFFT

以下はパラメーターの説明です。

-   **トラックバー・チェックボックス**
    -   Channel: IFFT の実行結果の出力先画像チャンネル [All, Mono, Red, Green, Blue, Alpha]
    -   Re<->Im: 入力する画像が実部・虚部いずれか（Channel == \"All\"のみ有効; その他では R で実部・G で虚部としてロードします。
    -   SwapQ:　周波数成分の象限入れ替えを行うかどうか
    -   InCurve: 入力値に対する変換の種類 [Linear, Log2]
    -   Enable: IFFT を実行するかどうか
    -   ShowHelp: ヘルプを表示するかどうか

---

## 導入方法 / how to install

[こちらのリポジトリ](https://github.com/Aodaruma/Aodaruma-AviUtl-Script)を参照してください。

## ライセンス / Licence

[こちらのリポジトリ](https://github.com/Aodaruma/Aodaruma-AviUtl-Script)を参照してください。
