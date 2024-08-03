#!/bin/bash

# 获取所有已修改的C/C++文件
files=$(git status --porcelain | grep -E '\.(c|cpp|h|hpp)$' | awk '{print $2}')

# 循环遍历文件并格式化
for file in $files; do
  echo ${file}
  clang-format -i "$file"
done
