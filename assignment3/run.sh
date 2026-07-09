#!/bin/bash

set -e

cd "$(dirname "$0")/test"


clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S loop.c -o loop.ll

opt -p mem2reg loop.ll -S -o loop.m2r.ll
opt -load-pass-plugin ../build/libmylicm.so -p my-licm -S loop.m2r.ll -o loop.final.ll