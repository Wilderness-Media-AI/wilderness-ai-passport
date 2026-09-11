<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# WILDERNESS AI Passport

这是一个可复用的 AI Passport 项目，把 FoloToy AI Passport 同时变成 WILDERNESS 员工数字工牌，以及控制 Mac 语音输入的随身麦克风。

## 两个功能

### 工牌模式

- 固定三页模板：员工身份页、中文服务页、品牌页。
- 每名员工只更换头像、姓名、职位和微信二维码。
- 长按 `OK` 展示员工微信二维码。
- 普通页面 3 秒无操作后降到 10% 亮度；低亮后的第一次完整按键手势只负责唤醒，不触发翻页或其他动作。
- 机身里的 NTAG213 NFC 是独立被动标签，不受固件控制。需要另外写入 `https://wmedia.cc/` 或每张工牌独立的跳转链接。

### AI 语音输入模式

- 在工牌模式长按 `UP` 进入语音页。
- 按住 `UP` 说话：AI Passport 使用 ES8311 麦克风收音并通过低功耗蓝牙传给 Mac，同时 Mac companion 代按豆包输入法所需的左 Control。
- 松开 `UP` 停止，按 `DOWN` 发送回车，短按 `OK` 返回工牌首页。
- 语音页 60 秒无操作后才降亮。
- 音频为 16 kHz、16-bit、单声道，每 100 ms 独立编码为 IMA-ADPCM 数据块。

当前 Mac companion 面向 macOS，已经通过 BlackHole 2ch 接入豆包输入法，也会保存 WAV 录音用于排查音频链路。

## 环境要求

- FoloToy AI Passport：ESP32-C3、8 MB Flash、无 PSRAM。
- 固件使用 ESP-IDF 5.5.3。
- macOS 13 或更新版本，电脑需支持蓝牙。
- BlackHole 2ch，并在豆包输入法里把它选为麦克风输入。
- 员工头像处理需要 Python 3、Pillow 和 macOS Vision。

## 快速开始

1. 激活 ESP-IDF 5.5.3。
2. 创建员工本地 profile。`private-profiles/` 整个目录都被 Git 忽略：

   ```bash
   python3 tools/provision_employee_badge.py \
     --slug employee-name \
     --name "Employee Name" \
     --role "Employee Role" \
     --portrait /path/to/portrait.jpg \
     --wechat-qr /path/to/wechat-qr.jpg \
     --build
   ```

3. 检查生成的 224 x 224 头像预览，并先在测试机核对二维码。
4. 使用保护脚本，只刷应用分区：

   ```bash
   ./tools/safe-flash-app.sh \
     --port /dev/cu.usbmodemXXXX \
     --app build-employee-employee-name/Wilderness-AI-Passport.bin
   ```

5. 构建并打开 Mac companion：

   ```bash
   ./companion-macos/build-app.sh
   open companion-macos/build/WildernessMic.app
   ```

6. 按 macOS 提示授予辅助功能与蓝牙权限。BlackHole 2ch 需要单独安装，然后在豆包输入法中选择 `BlackHole 2ch` 作为输入设备。

刷真实设备前请先看[部署与安全说明](docs/development/wilderness-deployment.zh_CN.md)和[员工工牌配置流程](docs/development/employee-badge-provisioning.zh_CN.md)。

## 默认安全规则

- 已配置的设备严禁运行 `idf.py erase-flash`。
- `cardid` 位于 `0x356000`，长度 `0x4000`，刷写前后必须逐字节一致。
- 保护脚本会先备份完整 8 MB Flash 和 `cardid`，只向 `0x10000` 写入应用，再校验写入结果并重新读取、比较 `cardid`。
- CI 只构建无员工资料的通用演示版。Release 不包含员工头像、姓名、微信二维码、设备身份、录音或设备备份。
- NFC 只能作为公开网页入口，不能作为门禁或可信身份凭证。

## 验证

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
swiftc -warnings-as-errors companion-macos/WildernessMic.swift \
  -framework AppKit -framework AudioToolbox -framework AVFoundation \
  -framework CoreBluetooth -o /tmp/WildernessMic
```

固件构建通过不等于真机验收。每台设备还需检查启动日志、屏幕、按键、蓝牙重连、麦克风音频、豆包实际落字、低亮策略和受保护的 `cardid`。

## 许可与署名

软件使用 MIT License，基于 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport) 开发。详见 [LICENSE](LICENSE) 与[第三方说明](THIRD_PARTY_NOTICES.zh_CN.md)。WILDERNESS MEDIA 品牌素材不授予商标使用权。
