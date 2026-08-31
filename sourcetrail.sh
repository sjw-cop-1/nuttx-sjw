#!/usr/bin/env bash
# NuttX 项目内的 Sourcetrail 启动脚本
#
# 用法（在仓库根目录执行）：
#   ./sourcetrail.sh                启动 Sourcetrail GUI 并打开本 NuttX 项目
#   ./sourcetrail.sh index --full   命令行重建索引（GUI 不开）
#   ./sourcetrail.sh <其他参数>      透传给 Sourcetrail
#
# 说明：
#   - 实际程序是 ~/apps/Sourcetrail.AppImage，DPI 适配在 ~/apps/sourcetrail.sh
#   - 工程文件 nuttx.srctrlprj 位于仓库上一级目录（nuttxspace/ 下）

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT="${SCRIPT_DIR}/../nuttx.srctrlprj"

if [ ! -f "$PROJECT" ]; then
  echo "[sourcetrail] 未找到工程文件: $PROJECT" >&2
  echo "[sourcetrail] 请先创建/恢复索引工程，或用: ~/apps/sourcetrail.sh 手动打开" >&2
  exit 1
fi

if [ $# -eq 0 ]; then
  # 无参数：启动 GUI 并直接打开 NuttX 项目
  exec ~/apps/sourcetrail.sh "$PROJECT"
else
  # 有参数（如 index）：原样透传
  exec ~/apps/sourcetrail.sh "$@"
fi
