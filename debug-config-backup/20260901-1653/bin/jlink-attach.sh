#!/usr/bin/env bash
# =============================================================================
# jlink-attach.sh —— 确保 J-Link 已从 Windows 透传进 WSL(usbip)
#
# 用途: VS Code 按 F5 调试前自动调用(见 .vscode/tasks.json 的
#       "J-Link: 确保透传" 任务)。也可手动运行。
#
# 背景: 本机 usbipd-win 4.3 的 `usbipd attach --wsl` 会因
#       "Mounting 'C:\Program Files\usbipd-win\WSL' within WSL failed" 失败,
#       所以这里绕过它, 直接用 usbipd 自带的 Linux usbip 客户端连
#       Windows 端 127.0.0.1:3240 (WSL mirrored 网络模式下可直连)。
#
# 幂等: 已透传则秒退; 未透传则挂载 helper → 找 busid → attach → 放权限。
#       遇到 "Device busy (exported)"(Windows 侧残留 Attached 状态)会自动
#       `usbipd detach` 清理后重试。
# 退出码: 0 = J-Link 在 WSL 内可用; 非 0 = 失败(F5 会中止, 不会启动 GDB Server)
# =============================================================================
set -u

VID_PID="1366:1020"                       # SEGGER J-Link
HELPER_DIR="/usr/lib/usbipd-win"
HELPER_SRC='C:\Program Files\usbipd-win\WSL'
USBIP="$HELPER_DIR/usbip"
REMOTE="127.0.0.1"                         # mirrored 网络: = Windows 主机
WAIT_SECS=10

log()  { printf '[jlink-attach] %s\n' "$*"; }
die()  { printf '[jlink-attach] ERROR: %s\n' "$*" >&2; exit 1; }

# 在 Windows 侧跑 usbipd(优先 PATH 里的 usbipd.exe, 退回全路径 powershell)
win_usbipd() {
  if command -v usbipd.exe >/dev/null 2>&1; then
    usbipd.exe "$@"
  else
    powershell.exe -NoProfile -Command "usbipd $*" 2>&1
  fi
}

have_jlink() { lsusb | grep -qi "$VID_PID"; }

# ---------------------------------------------------------------------------
# 0. 已经在 WSL 里了? 直接结束
# ---------------------------------------------------------------------------
if have_jlink; then
  log "J-Link 已透传 ($(lsusb | grep -i "$VID_PID" | sed 's/^/  /'))"
  sudo chmod 666 /dev/bus/usb/*/* 2>/dev/null || true
  exit 0
fi

log "J-Link 不在 WSL 内, 开始透传 ..."

# ---------------------------------------------------------------------------
# 1. 内核模块 + helper 目录挂载
# ---------------------------------------------------------------------------
sudo modprobe vhci-hcd 2>/dev/null || true

if ! mountpoint -q "$HELPER_DIR"; then
  sudo mkdir -p "$HELPER_DIR"
  sudo mount -t drvfs "$HELPER_SRC" "$HELPER_DIR" 2>/dev/null \
    || die "挂载 $HELPER_SRC 失败 —— Windows 端是否装了 usbipd-win?"
fi
[ -x "$USBIP" ] || die "找不到 $USBIP —— usbipd-win 版本不对?"

# ---------------------------------------------------------------------------
# 2. 从 Windows 端列出可导出设备, 按 VID:PID 找 busid(换 USB 口也不怕写死失效)
# ---------------------------------------------------------------------------
LIST="$(sudo "$USBIP" list -r "$REMOTE" 2>/dev/null)" \
  || die "连不上 Windows usbipd ($REMOTE:3240) —— usbipd 服务没起/防火墙?"

BUSID="$(printf '%s\n' "$LIST" | grep -i "($VID_PID)" | head -n1 | sed -E 's/^ *([0-9]+-[0-9]+):.*/\1/')"

if [ -z "$BUSID" ]; then
  cat >&2 <<EOF
[jlink-attach] ERROR: Windows 端没有共享 J-Link ($VID_PID)。
  在 Windows 管理员 PowerShell 里执行一次(永久生效):
      usbipd bind --busid <J-Link的BUSID>
  查 BUSID: usbipd list
EOF
  exit 1
fi
log "找到 J-Link: busid $BUSID"

# ---------------------------------------------------------------------------
# 3. attach(带 "Device busy" 自愈: usbipd detach 清残留后重试)
# ---------------------------------------------------------------------------
attach_once() {
  ATTACH_OUT="$(sudo "$USBIP" attach -r "$REMOTE" -b "$BUSID" 2>&1)"
  local rc=$?
  printf '%s\n' "$ATTACH_OUT"
  return $rc
}

for try in 1 2 3; do
  OUT="$(attach_once)"; RC=$?

  if [ $RC -eq 0 ] || printf '%s' "$OUT" | grep -qi "already in use"; then
    break
  fi

  if printf '%s' "$OUT" | grep -qiE "busy|exported|already"; then
    log "usbip 返回忙/已导出(Windows 侧残留 Attached),尝试 usbipd detach 清理 ..."
    win_usbipd detach --busid "$BUSID" >/dev/null 2>&1 || true
    sleep 2
    continue
  fi

  # 其它错误直接退出
  die "usbip attach 失败: $OUT"
done

# ---------------------------------------------------------------------------
# 4. 等设备在 WSL 里出现
# ---------------------------------------------------------------------------
for i in $(seq 1 "$WAIT_SECS"); do
  if have_jlink; then
    sudo chmod 666 /dev/bus/usb/*/* 2>/dev/null || true
    log "透传成功: $(lsusb | grep -i "$VID_PID")"
    exit 0
  fi
  sleep 1
done

# 最后一搏: 强制 detach + 再 attach 一次
log "${WAIT_SECS}s 内未出现,最后强制 detach 重试一次 ..."
win_usbipd detach --busid "$BUSID" >/dev/null 2>&1 || true
sleep 2
attach_once >/dev/null 2>&1 || true
for i in $(seq 1 "$WAIT_SECS"); do
  if have_jlink; then
    sudo chmod 666 /dev/bus/usb/*/* 2>/dev/null || true
    log "透传成功(重试后): $(lsusb | grep -i "$VID_PID")"
    exit 0
  fi
  sleep 1
done

die "J-Link 仍未出现。手动排查: Windows 端 'usbipd detach --busid $BUSID' 再重试;或拔插 J-Link。"
