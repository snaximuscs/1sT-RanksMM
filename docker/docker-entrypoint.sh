#!/bin/bash
set -e

git submodule update --init --recursive

SDK_DIR="/tmp/sdk"
HL2SDK_DIR="$SDK_DIR/hl2sdk-cs2"
MMSOURCE_DIR="$SDK_DIR/metamod-source"
SCHEMA_DIR="/app/SchemaEntity"

echo "=== Preparing temporary SDK directory ==="
rm -rf "$SDK_DIR" "$SCHEMA_DIR"
mkdir -p "$SDK_DIR"

echo "=== Downloading HL2SDK-CS2 ==="
git clone --recursive --branch cs2 --single-branch https://github.com/alliedmodders/hl2sdk.git "$HL2SDK_DIR"

echo "=== Downloading Metamod-Source ==="
git clone --recursive --branch master --single-branch https://github.com/alliedmodders/metamod-source.git "$MMSOURCE_DIR"

echo "=== Downloading SchemaEntity ==="
git clone https://github.com/pisex/SchemaEntity.git "$SCHEMA_DIR"

export HL2SDKCS2="$HL2SDK_DIR"
export MMSOURCE_DEV="$MMSOURCE_DIR"

echo "Using HL2SDKCS2=$HL2SDKCS2"
echo "Using MMSOURCE_DEV=$MMSOURCE_DEV"

echo "=== Starting build ==="
rm -rf build
mkdir build
cd build

python ../configure.py \
  --enable-optimize \
  --sdks cs2 \
  --targets x86_64 \
  --mms_path="$MMSOURCE_DEV" \
  --hl2sdk-root="$SDK_DIR" \
  --hl2sdk-manifests="$MMSOURCE_DEV/hl2sdk-manifests"

ambuild

echo "=== DONE ==="
