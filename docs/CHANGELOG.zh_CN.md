<p align="right">
  <strong>简体中文</strong> · <a href="CHANGELOG.md">English</a>
</p>

# 更新记录

## 未发布

- 调整 BLE 麦克风生命周期：`WildernessMic` 仅在语音输入页广播并接受 Mac 连接；返回工牌时会停止采音、断开 Mac、关闭音频 codec 并停止 BLE 广播。
- 保护式应用分区刷写现在必须提供目标设备 MAC，并允许调整备份与回读波特率。

## 0.1.0 - 2026-09-11

- 增加固定三页的 WILDERNESS 员工工牌，头像、姓名、职位和微信二维码只在本地私有 profile 中配置。
- 增加全员统一的中文服务页与 WILDERNESS 品牌页。
- 工牌模式 3 秒后降至 10% 亮度，语音模式延长至 60 秒；低亮后的第一次完整按键手势只负责唤醒。
- 增加工牌按键控制的语音输入：长按 `UP` 进入语音页，按住 `UP` 传输麦克风音频，`DOWN` 发送回车，短按 `OK` 返回工牌。
- 增加原生 macOS 低功耗蓝牙 companion、IMA-ADPCM 解码、诊断 WAV、通过 BlackHole 2ch 输出虚拟麦克风，以及豆包左 Control 控制。
- 增加员工私有 profile 生成、安全的 application-only 刷写、host tests、固件布局校验、CI 和中英文部署文档。
- 固件基于 FoloToy AI Passport commit `f75873f1aab24ac4c0ba9394c131669f66cce650`，保留 MIT 署名。
