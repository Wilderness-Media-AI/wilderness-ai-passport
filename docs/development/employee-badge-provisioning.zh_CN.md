<p align="right">
  <strong>简体中文</strong> · <a href="employee-badge-provisioning.md">English</a>
</p>

# WILDERNESS 员工工牌配置流程

员工工牌采用一套固定产品模板。三个普通页面、中文服务页、品牌页、颜色、字体层级、电量位置、二维码尺寸和按键逻辑在所有设备上保持一致。第二页固定使用 `品牌社交`、`直播`、`AIGC 视频`、`品牌 TVC`、`SaaS 产品教程视频` 五项中文服务名称。普通工牌页无操作 3 秒后背光降至 10%，语音输入页则延长为 60 秒；低亮状态下首次完整按键手势只恢复全亮。这是背光调暗，不是 deep sleep。长按 `UP` 进入语音页，按住/松开 `UP` 开始/停止录音，`DOWN` 发送，短按 `OK` 返回工牌。每名员工只允许更换头像、显示姓名、职位和微信二维码。

## 私有资料目录约定

每名员工使用一个本地 `private-profiles/<slug>/` 目录。头像和二维码属于个人数据，整个目录均被 Git 忽略。完整资料目录包含：

- `badge_profile_private.h`：姓名、职位和二维码页标签。
- `badge_avatar_private.h` 与 `badge_avatar_private.c`：嵌入固定 WM Logo 头像框的 224 × 224 RGB565 头像。
- `wechat_qr_private.h`：二维码私有数据；配置工具不会打印二维码内容。
- `badge-avatar-private-224.png`：用于目视检查的预览图。
- 本地头像原图、透明抠图、二维码原图和 `profile.json` 元数据。

不得把这些文件复制到 `main/`、文档、发布包或版本控制中。

## 新建或更新员工资料

在 macOS 上按需激活项目 Python 环境，然后运行：

```bash
python3 tools/provision_employee_badge.py \
  --slug employee-name \
  --name "Employee Name" \
  --role "Employee Role" \
  --portrait /absolute/path/to/portrait.jpg \
  --wechat-qr /absolute/path/to/wechat-qr.jpg \
  --build
```

更新已有员工时增加 `--replace`。工具会先把旧私有目录移动到 `private-profiles/.backups/`，再进行替换。该命令会把姓名和职位统一为大写，拒绝超出固定字体版式的文字，使用 macOS Vision 生成真实透明人像，把人像合成进 WM Logo，要求二维码图片中恰好存在一个可解码二维码，并构建员工专属固件、校验受保护 Flash 布局。

手动构建时必须显式传入 `-D BADGE_PROFILE_DIR=/absolute/path/to/private-profiles/<slug>`，并在 CMake 输出、`CMakeCache.txt` 和 `compile_commands.json` 中确认目标 profile。没有这个证据的构建不得用于员工设备。

固件现在会在既未指定私有 profile、也未显式传入
`-D BADGE_ALLOW_GENERIC=ON` 时直接终止配置。通用版开关只供 CI/社区验证，
严禁刷入员工设备。旧的人物专属头像隐式回退已删除，因为它可能把某一员工头像与通用姓名文字错误混用。

## 视觉与真机验收

刷写前检查 `badge-avatar-private-224.png`：头部、脖子、衣领和肩部必须完整；人物必须位于 Logo 内部留白，不能接触 `WM` 或 `media`。同时确认姓名保持单行、职位清晰可读。

每台设备都必须执行：

1. 识别 USB 设备并确认是目标员工对应的 MAC 地址。
2. 回读并备份完整 8 MB Flash，以及 `0x356000` 起始、长度 `0x4000` 的 `cardid`。
3. 严禁使用 `erase-flash`。只根据该员工构建目录的 `flash_args` 写入 bootloader `0x0`、partition table `0x8000` 和 application `0x10000`。
4. 刷写后再次回读 `cardid`，必须与刷写前备份逐字节一致。
5. 确认启动过程没有 panic 或重启循环，再目视检查三个页面，并多次长按 `OK` 检查固定为 232 × 232 的二维码页。等待 3 秒确认背光降至 10%，第一次按键只恢复全亮不翻页，并再等待一次确认可重复调暗。长按 `UP` 进入语音输入页，确认页面保持全亮 60 秒，并可以短按 `OK` 返回。
6. 使用微信扫码，确认二维码属于目标员工。

固件构建通过不等于真机验收完成。每台设备应分别记录固件构建、目标 MAC、刷写前后 `cardid` 哈希、写入区段校验、启动日志、屏幕目视结果和二维码扫码结果。
