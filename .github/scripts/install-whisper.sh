#!/usr/bin/env bash
# Install Whisper RISC-V ISA Simulator from source.
# Usage: install-whisper.sh <install-dir>
# Update WHISPER_TAG to track a newer release; the CI cache key is derived
# from this script's content hash, so any edit here invalidates the cache.

set -euo pipefail

INSTALL_DIR="${1:?Usage: install-whisper.sh <install-dir>}"
WHISPER_TAG="1.861"

# ---- Build dependencies ------------------------------------------------------
# g++ >= 11 (whisper builds with -std=c++20), Boost headers + program_options,
# and zlib (whisper links -lz). Whisper's GNUmakefile defaults to STATIC_LINK=1,
# so libboost_program_options.a is linked statically and the resulting binary
# has no Boost runtime dependency.
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    libboost-dev \
    libboost-program-options-dev \
    zlib1g-dev

# Whisper's GNUmakefile expects BOOST_ROOT to provide include/ and lib/
# sub-directories and adds them via -isystem. Ubuntu spreads headers and
# libraries across /usr/include and /usr/lib/x86_64-linux-gnu, so build a
# shim directory with the expected layout. Note: only symlink the boost
# headers, NOT all of /usr/include — passing /usr/include as -isystem
# breaks the include_next chain of the C++ standard headers.
sudo mkdir -p /opt/boost/include
sudo ln -sfn /usr/include/boost /opt/boost/include/boost
sudo ln -sfn /usr/lib/x86_64-linux-gnu /opt/boost/lib

# ---- Build and install -------------------------------------------------------
git clone --depth 1 --branch "$WHISPER_TAG" \
    https://github.com/tenstorrent/whisper.git /tmp/whisper
cd /tmp/whisper
BOOST_ROOT=/opt/boost make -j"$(nproc)"
mkdir -p "$INSTALL_DIR/bin"
cp build-Linux/whisper "$INSTALL_DIR/bin/"
