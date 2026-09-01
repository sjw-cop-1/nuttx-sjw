# M144Z-M3 VS Code 调试配置备份 — 2026-09-01 16:53

NuttX + STM32F103ZET6 + J-Link + WSL2 的一键 F5 调试环境快照。
`.vscode/` 被 git 忽略,所以单独在这里存档。

## 包含内容

| 文件 | 恢复到 | 说明 |
|------|--------|------|
| `vscode/launch.json` | `<repo>/.vscode/launch.json` | Cortex-Debug + cppdbg 两套配置,含 BOOT0 兜底 |
| `vscode/tasks.json` | `<repo>/.vscode/tasks.json` | 编译 / J-Link 透传 / GDB Server / 复合任务 |
| `vscode/c_cpp_properties.json` | `<repo>/.vscode/` | IntelliSense (compile_commands.json) |
| `vscode/settings.json` | `<repo>/.vscode/` | debug.onTaskErrors=abort, cortex-debug 路径 |
| `vscode/extensions.json` | `<repo>/.vscode/` | 推荐扩展列表 |
| `vscode/STM32F103xx.svd` | `<repo>/.vscode/` | 外设寄存器视图数据 |
| `bin/jlink-attach.sh` | `~/bin/jlink-attach.sh` | 保证 J-Link 透传进 WSL(F5 preLaunchTask 调用) |
| `bin/usbip-debug-boot.sh` | `~/bin/usbip-debug-boot.sh` | 开机自动透传(/etc/wsl.conf [boot] 调用) |

## 恢复

```bash
cd /home/nuttx/nuttxspace/nuttx/debug-config-backup/20260901-1653
bash restore.sh
```

或手动:

```bash
D=/home/nuttx/nuttxspace/nuttx/debug-config-backup/20260901-1653
cp "$D"/vscode/* /home/nuttx/nuttxspace/nuttx/.vscode/
cp "$D"/bin/*.sh ~/bin/ && chmod +x ~/bin/jlink-attach.sh ~/bin/usbip-debug-boot.sh
```

## 环境依赖(不在备份里,需另行确认)

- WSL2 `networkingMode=mirrored`(`~/.wslconfig`)
- `/usr/local/bin/JLinkGDBServerCLI`(SEGGER J-Link 软件包)
- `/usr/bin/gdb-multiarch`, `binutils-arm-none-eabi`, `gcc-arm-none-eabi`, `bear`
- VS Code 扩展: `marus25.cortex-debug` + `mcu-debug.*`(见 extensions.json)
- Windows: `usbipd-win` 已 `usbipd bind` J-Link
- `nuttx ALL=(ALL) NOPASSWD:ALL`(jlink-attach.sh 用 sudo)
- 板子 `./tools/configure.sh m144z-m3:nsh` 已配置

## 已知硬件问题(见 launch.json 顶部注释)

板子 BOOT0=1,复位进 ST bootloader。launch.json 里的 `overrideLaunchCommands` /
`overrideResetCommands` 是软件兜底(从 Flash 向量表载入 SP/PC)。
**把板上 BOOT0 跳线拨到 GND 后,应删掉这几段 override。**
