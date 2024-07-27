#!/bin/bash

# 定义节点数量
NUM_NODES=3

# 循环启动每个节点
for ((i=1; i<=NUM_NODES; i++))
do
  echo "Starting node with id: $i"
  ./build/node --id $i 1>>./tmp/normal.txt 2>>./tmp/error.txt &

done

# 等待所有节点启动完成
wait

echo "All nodes have been started."
