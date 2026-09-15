<p align="right">
  <strong>简体中文</strong> · <a href="CHANGELOG.md">English</a>
</p>

# 更新记录

## 未发布

- 调整 BLE 麦克风生命周期：`Wilderness Mic` 仅在语音输入页广播并接受 Mac 连接；返回工牌或进入 Radio 时会停止采音、断开 Mac 并停止 BLE 广播，避免 2.4 GHz 共存干扰。
- 增加 V2 低功耗 ESP-NOW 呼叫待机：长按 `DOWN` 发送约 1.5 秒呼叫，附近兼容工牌收到后自动亮屏并进入 Radio，接收方无需先手动进入页面。
- 增加双向 `CALL_ACK` 和 `HANGUP`；任意一方短按 `OK` 都会让双方回到待机，30 秒无语音活动也会自动结束。
- 增加同时抢麦裁决、对端超时与恢复、独立 25 ms IMA-ADPCM 帧，并与蓝牙麦克风模式共用音频占用管理。
- 对讲页沿用 60 秒后降亮。V2 仍是未加密的固定广播房间，仅用于两台设备实验室验证。
- 保护式应用分区刷写现在必须提供目标设备 MAC，并允许调整备份与回读波特率。

## 0.1.0 - 2026-09-11

- 增加固定三页的 WILDERNESS 员工工牌，头像、姓名、职位和微信二维码只在本地私有 profile 中配置。
- 增加全员统一的中文服务页与 WILDERNESS 品牌页。
- 工牌模式 3 秒后降至 10% 亮度，语音模式延长至 60 秒；低亮后的第一次完整按键手势只负责唤醒。
- 增加工牌按键控制的语音输入：长按 `UP` 进入语音页，按住 `UP` 传输麦克风音频，`DOWN` 发送回车，短按 `OK` 返回工牌。
- 增加原生 macOS 低功耗蓝牙 companion、IMA-ADPCM 解码、诊断 WAV、通过 BlackHole 2ch 输出虚拟麦克风，以及豆包左 Control 控制。
- 增加员工私有 profile 生成、安全的 application-only 刷写、host tests、固件布局校验、CI 和中英文部署文档。
- 固件基于 FoloToy AI Passport commit `f75873f1aab24ac4c0ba9394c131669f66cce650`，保留 MIT 署名。
