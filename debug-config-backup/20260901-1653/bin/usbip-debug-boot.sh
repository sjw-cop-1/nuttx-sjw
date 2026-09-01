#!/usr/bin/env bash
# =============================================================================
# 开机自动(由 /etc/wsl.conf [boot] 调用, root 运行):
#   1) 挂载 usbipd-win 的 WSL 侧载目录(usbipd 自己挂载会失败, 需手动)
#   2) 后台循环: 调用 ~/bin/jlink-attach.sh 保证 J-Link 透传(按 VID:PID 找
#      busid, 换 USB 口也不怕; 已透传则秒退)
#   3) 循环放开 /dev/bus/usb 权限(J-Link 工具需要写权限, WSL 无 udev)
#
# 注: 即使这个脚本没生效, VS Code 按 F5 时的 "J-Link: 确保透传" 任务
#     也会兜底把 J-Link 拉进来。
# =============================================================================
sleep 5

mkdir -p /usr/lib/usbipd-win
mountpoint -q /usr/lib/usbipd-win || \
  mount -t drvfs 'C:\Program Files\usbipd-win\WSL' /usr/lib/usbipd-win 2>/dev/null

ATTACH_SH=/home/nuttx/bin/jlink-attach.sh

# 后台: 每 5s 确保 J-Link 在线(幂等, 在线时几乎零开销)
nohup sh -c "while true; do bash $ATTACH_SH >/tmp/usbip-attach.log 2>&1; sleep 5; done" \
  >/dev/null 2>&1 &

# 后台: 持续放开 USB 设备节点权限
nohup sh -c 'while true; do chmod 666 /dev/bus/usb/*/* 2>/dev/null; sleep 5; done' \
  >/dev/null 2>&1 &

exit 0
