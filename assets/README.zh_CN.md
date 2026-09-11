<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

### WILDERNESS WM Logo

- 来源：WILDERNESS 官网项目内公司自有的正式素材 `public/assets/home-wm-logo.svg`。
- 保存的源文件：`images/wilderness-wm-logo.svg`。
- 生成预览：`images/wilderness-wm-logo-192.png`，192 × 192，黑色背景。
- 固件产物：`main/wilderness_logo.c` 与 `main/wilderness_logo.h`，采用供 LVGL 使用的 RGB565 字节交换格式。
- 重新生成：`python3 tools/generate_wilderness_logo.py <official-home-wm-logo.svg>`；开发 Mac 需具备 ImageMagick 与 Pillow。

### 本地私人工牌头像

- 个人头像源文件与生成资源只保存在本地并由 Git 忽略，严禁提交。
- 使用 `python3 tools/generate_private_avatar.py <transparent-portrait.png>` 从真实 RGBA 透明抠图生成 152 × 152 RGB565 资源。人物只出现在正式 WM Logo 的米白内部，黑色 `WM/media` 图形保持在人物上层。
- 旧版本地固件产物为 `main/badge_avatar_private.c` 与 `.h`；员工配置流程会把私有产物写入 Git 忽略的 `private-profiles/` 目录。

### 固定中文服务页

- 产品文案：将官网五项服务分类固定翻译为 `品牌社交`、`直播`、`AIGC 视频`、`品牌 TVC`、`SaaS 产品教程视频`；属于公司自有产品内容。
- 生成预览：`images/wilderness-services-cn-240x320.png`，尺寸为 240 × 320。
- 固件产物：`main/wilderness_services_cn.c` 与 `main/wilderness_services_cn.h`，采用供 LVGL 使用的 RGB565 字节交换格式。
- 重新生成：`python3 tools/generate_wilderness_services_cn.py`。开发 Mac 使用系统自带 STHeiti 字体栅格化固定文案，仓库不保存或分发字体文件。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
