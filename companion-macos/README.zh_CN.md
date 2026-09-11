<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Wilderness Mic for macOS

这个原生 companion 通过低功耗蓝牙连接 AI Passport，解码 16 kHz 单声道 IMA-ADPCM 音频，保存诊断 WAV；如果电脑已安装 BlackHole 2ch，还会把实时 PCM 送入 BlackHole。

工牌按键会控制 Mac 当前应用：

- 工牌 `UP` 的按下和松开会代按、释放豆包输入法使用的左 Control。
- 工牌 `DOWN` 会发送回车。
- Mac 实体左 Control 仍可用于开始和停止诊断录音。

构建并打开 App：

```bash
./build-app.sh
open build/WildernessMic.app
```

首次打开时授予蓝牙和辅助功能权限。BlackHole 2ch 需要单独安装，并在语音输入软件内部选为麦克风。companion 不会安装驱动，也不会修改系统默认输入。

直接从终端运行二进制时仍可使用 `start`、`stop` 和 `record 3` 诊断命令。WAV 文件会写入 `~/Library/Application Support/WildernessMic/recordings/`。
