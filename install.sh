#!/usr/bin/env bash

# This script is only available for the debian series

# update
sudo apt update
sudo apt upgrade

# install environments
sudo apt install -y gcc g++ gdb cmake
sudo apt install -y llvm-12-dev libclang-12-dev clang-12
sudo apt install -y build-essential
sudo apt install -y libedit-dev
sudo apt install -y libboost-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-thread-dev libboost-regex-dev
sudo apt install -y libgtk-3-dev libgtkmm-3.0-dev libgtksourceviewmm-3.0-dev
sudo apt install -y libmysqlcppconn-dev
sudo apt install -y openssl libssl-dev
sudo apt install -y pkg-config
sudo apt install -y ninja-build
sudo apt install -y sqlite3 libsqlite3-dev

cmake -B $(pwd)/build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build $(pwd)/build
sudo cmake --build $(pwd)/build --target install

echo "Now you can give it a try"