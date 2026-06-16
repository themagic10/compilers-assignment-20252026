#!/bin/bash

set -e

cd "$(dirname "$0")/test"


clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S foo.c -o foo.ll

opt -p mem2reg foo.ll -S -o foo.m2r.ll
opt -load-pass-plugin ../build/libalgid.so -p alg-id -S foo.m2r.ll -o foo.final.ll