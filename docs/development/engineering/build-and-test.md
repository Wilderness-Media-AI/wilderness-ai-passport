<p align="right">
  <a href="build-and-test.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Build and Test

Use ESP-IDF 5.5.3. On a clean machine or when the toolchain is missing, follow
the [environment bootstrap](environment-setup.md) first.

> Prefer `./tools/validate.sh --firmware` for firmware builds. It validates a
> merged image internally, but publishes only `build/Wilderness-AI-Passport.bin`.
> On a provisioned device, use `tools/safe-flash-app.sh`. Treat
> `idf.py build` and `idf.py flash` as incremental development commands, not the
> default delivery path.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version             # must report ESP-IDF v5.5.3
./tools/validate.sh --firmware # verify layout and export application image
idf.py set-target esp32c3     # fresh checkout or changed target
idf.py build                  # optional incremental application build
idf.py flash monitor          # optional incremental application flash
idf.py fullclean              # remove stale generated build state only
```

`idf.py fullclean` does not fully synchronize an existing `sdkconfig` with
changed defaults. Preserve intentional local settings, then run
`idf.py set-target esp32c3` when the target or tracked defaults must be
regenerated.

The tracked `dependencies.lock` pins Managed Component resolution. After changing an `idf_component.yml`, regenerate the lock with ESP-IDF 5.5.3, review version changes, and commit it with the manifest. An ordinary build must not leave an unexplained lock-file diff.

Firmware validation uses a fresh temporary build directory and an isolated `sdkconfig` generated from the tracked defaults. It does not consume or overwrite a developer's root `sdkconfig`. It verifies a temporary merged image, then copies only the application image to `build/Wilderness-AI-Passport.bin` for distribution. The gate also enforces the [protected Flash layout](protected-flash-layout.md): the protected `cardid` address, application size, partition-table MD5, and absence of device-specific identity data.

The baseline also has a hardware-independent logic test:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

Use the unified validation entry point:

```bash
./tools/validate.sh --static    # repository checks, workflows, links, secrets, host tests
./tools/validate.sh --firmware  # build, merge-bin, offsets, and protected layout
./tools/validate.sh             # complete gate; requires an activated ESP-IDF environment
```

CI calls the same script. Fix the shared script or environment if local and CI behavior differs; do not duplicate command sequences in workflows.

Hardware-affecting changes must also run the applicable on-device checklist in the hardware guide. Report compilation separately from physical-device validation.

Distribute only `build/Wilderness-AI-Passport.bin` for existing devices and flash it
at `0x10000` with `tools/safe-flash-app.sh`. Keep merged images local for factory
diagnostics on blank hardware.
