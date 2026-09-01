#!/usr/bin/env bash
# 恢复本快照到工作区。用法: bash restore.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="/home/nuttx/nuttxspace/nuttx"

echo "从 $HERE 恢复到 $REPO/.vscode 和 ~/bin ..."
mkdir -p "$REPO/.vscode"
cp -v "$HERE"/vscode/* "$REPO/.vscode/"
cp -v "$HERE"/bin/*.sh "$HOME/bin/"
chmod +x "$HOME/bin/jlink-attach.sh" "$HOME/bin/usbip-debug-boot.sh"
echo
echo "完成。若刚改过 launch.json,在 VS Code 里 reload window。"
