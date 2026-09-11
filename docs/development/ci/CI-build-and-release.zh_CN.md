<p align="right">
  <strong>简体中文</strong> · <a href="CI-build-and-release.md">English</a>
</p>

# 自动构建与发布

`.github/workflows/firmware-checks.yml` 会用 ESP-IDF 5.5.3 检查每次进入 `main` 的修改和每个 pull request。`.github/workflows/build-firmware.yml` 用于 tag 构建，也支持手动触发。

固件门禁会在临时目录生成合并镜像，检查 bootloader、分区表、应用偏移、8 MB Flash 参数、3 MB 应用上限和受保护的 `cardid` 区域，然后只输出 `build/Wilderness-AI-Passport.bin`。

GitHub Release 只包含通用的 application-only 镜像，不包含员工 profile、头像、微信二维码、设备备份、录音或 `cardid`。必须通过 `tools/safe-flash-app.sh` 把它写入 `0x10000`，不能写入 `0x0`。

真实员工固件必须从 Git 忽略的本地私有 profile 构建，公开 CI 不会生成员工版本。

所有 GitHub Actions 都固定到完整 commit SHA。构建任务只有仓库只读权限，只有 tag release job 获得 `contents: write`。
