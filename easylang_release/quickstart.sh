#!/usr/bin/env bash
set -e
CXX="${CXX:-g++}"
echo "[EasyLang] Building..."
"$CXX" -O3 -std=c++17 -o ./el ./src/main.cpp
echo "[EasyLang] Done!  Run:  ./el your_script.el"
