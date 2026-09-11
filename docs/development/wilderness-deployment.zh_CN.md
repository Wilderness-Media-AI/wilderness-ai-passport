<p align="right">
  <strong>简体中文</strong> · <a href="wilderness-deployment.md">English</a>
</p>

# WILDERNESS 部署与验收

## 1. 创建员工私有 profile

在 macOS 上运行 `tools/provision_employee_badge.py`。头像、二维码内容、原图、生成的 C 数组和本地元数据都留在 Git 忽略的 `private-profiles/` 目录。构建前先检查 224 x 224 头像预览。

## 2. 构建并检查固件

使用 ESP-IDF 5.5.3。真实员工版本必须显式传入 profile：

```bash
idf.py -B build-employee-name \
  -D BADGE_PROFILE_DIR="$PWD/private-profiles/employee-name" build
idf.py -B build-employee-name merge-bin \
  -o build-employee-name/Wilderness-AI-Passport-full.bin
python3 tools/verify_firmware.py build-employee-name
```

合并镜像只用于工厂诊断，不用于员工设备的日常更新。日常更新必须用 application-only 镜像和 `tools/safe-flash-app.sh`。

## 3. 不触碰设备身份地刷写

一次只连接一台设备，并重新确认当前 `/dev/cu.usbmodem*` 端口。保护脚本会：

1. 读取设备 MAC。
2. 备份完整 8 MB Flash。
3. 回读 `0x356000` 起始、长度 `0x4000` 的 `cardid`。
4. 只向 `0x10000` 写入应用镜像。
5. 校验应用区写入内容。
6. 再次读取 `cardid`，要求刷前刷后逐字节一致。

已配置设备严禁运行 `idf.py erase-flash`。

## 4. 安装 macOS companion

单独安装 BlackHole 2ch，构建并打开 `WildernessMic.app`，授予蓝牙和辅助功能权限。然后在语音输入软件中选择 BlackHole 2ch 作为麦克风。companion 不会修改系统默认输入，也不会安装驱动。

## 5. 每台设备的验收项

- 启动日志无 panic、无循环重启。
- 身份页、中文服务页、品牌页、电量和大尺寸微信二维码显示正确。
- 普通页面 3 秒后降至 10% 亮度，第一次完整按键手势只唤醒，不误触功能。
- 长按 `UP` 进入语音页，页面 60 秒后才降亮；按住 `UP` 录音，`DOWN` 发送，短按 `OK` 返回。
- 蓝牙中断后 companion 能自动重连。
- 诊断 WAV 语音清晰，丢块数为 0。
- 豆包输入法能收到来自工牌麦克风的真实文字。
- 刷写前后的 `cardid` 文件逐字节一致。
- 单独写入的 NFC 链接能分别在 iPhone 和 Android 手机上打开正确网站。
