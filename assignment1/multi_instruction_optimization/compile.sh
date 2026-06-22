#!/bin/bash

set -e

LLVM_PATH="/usr/lib/llvm19/bin"

cd "$(dirname "$0")"

mkdir -p build

cd "./build"

cmake -DLT_LLVM_INSTALL_DIR=$LLVM_PATH ../src/

make

echo "compiled successfully"

