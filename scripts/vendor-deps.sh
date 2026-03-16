#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# scripts/vendor-deps.sh
#
# 在联网机器上运行，预拉取 corekit 依赖。
# - tinyxml2: 下载 zip 到 third_party/，供 CMake 自动解压。
# - glog/mimalloc/oneTBB: 下载源码 zip 并展开到 3party/，供离线构建直接使用。
#
# 用法：bash scripts/vendor-deps.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
DEST_DIR="$REPO_ROOT/third_party"
LOCAL_3PARTY_DIR="$REPO_ROOT/3party"
mkdir -p "$DEST_DIR"
mkdir -p "$LOCAL_3PARTY_DIR"

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

extract_to_3party() {
  local name="$1"
  local url="$2"
  local zip_path="$DEST_DIR/${name}.zip"
  local target_dir="$LOCAL_3PARTY_DIR/${name}"
  local tmp_dir

  download "$name" "$url"

  if [ -d "$target_dir" ]; then
    echo "[skip] ${target_dir} 已存在"
    return 0
  fi

  tmp_dir="$(mktemp -d)"
  echo "[extract] ${name} -> ${target_dir}"
  (
    cd "$tmp_dir"
    cmake -E tar xf "$zip_path"
  )
  local first_entry
  first_entry="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 | head -n 1)"
  if [ -z "$first_entry" ]; then
    echo "[error] ${name}.zip 解压后为空" >&2
    rm -rf "$tmp_dir"
    exit 1
  fi
  mv "$first_entry" "$target_dir"
  rm -rf "$tmp_dir"
}

download "tinyxml2" "https://github.com/leethomason/tinyxml2/archive/refs/tags/10.0.0.zip"
extract_to_3party "glog" "https://github.com/google/glog/archive/refs/tags/v0.7.1.zip"
extract_to_3party "mimalloc" "https://github.com/microsoft/mimalloc/archive/refs/tags/v2.1.7.zip"
extract_to_3party "oneTBB" "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.12.0.zip"

echo ""
echo "依赖已准备完成："
echo "  third_party/: tinyxml2 zip"
echo "  3party/: glog, mimalloc, oneTBB 源码"
echo ""
echo "下一步（提交到仓库）："
echo "  git add third_party/*.zip 3party/glog 3party/mimalloc 3party/oneTBB"
echo "  git commit -m 'vendor: add third-party dependency zips'"
