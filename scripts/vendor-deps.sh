#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# scripts/vendor-deps.sh
#
# 在联网机器上运行，下载所有三方依赖的 zip 包到 third_party/。
# 内网机器直接使用已入库的 zip，cmake 自动解压，无需执行此脚本。
#
# 用法：bash scripts/vendor-deps.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DEST_DIR="$REPO_ROOT/third_party"
mkdir -p "$DEST_DIR"

# 下载函数：幂等，已存在则跳过
download() {
  local name="$1"
  local url="$2"
  local out="$DEST_DIR/${name}.zip"
  if [ -f "$out" ]; then
    echo "[skip] ${name}.zip 已存在"
    return 0
  fi
  echo "[download] ${name}.zip ← $url"
  if command -v curl &>/dev/null; then
    curl -fL "$url" -o "$out"
  elif command -v wget &>/dev/null; then
    wget -q "$url" -O "$out"
  else
    echo "[error] 需要 curl 或 wget" >&2; exit 1
  fi
  local size
  size=$(du -sh "$out" | cut -f1)
  echo "[done] ${name}.zip: $size"
}

download "tinyxml2" "https://github.com/leethomason/tinyxml2/archive/refs/tags/10.0.0.zip"

echo ""
echo "所有依赖已下载到 $DEST_DIR/"
echo ""
echo "下一步（提交到仓库）："
echo "  git add third_party/*.zip"
echo "  git commit -m 'vendor: add third-party dependency zips'"
