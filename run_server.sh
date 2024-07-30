#!/bin/bash

# 定义节点数量
NUM_NODES=3
BASE="127.0.0."
addresses=""
for ((id=1; id<=NUM_NODES; id++))
do
  addresses+="${BASE}${id},"
done
# echo "${addresses}"
# 循环启动每个节点
for ((id=1; id<=NUM_NODES; id++))
do
  echo "Starting node with id: ${id}"
  ./build/weight_server $(pwd)/tmp/node_${id} ${id} ${addresses} ${BASE}${id} > tmp/std_$id.log 2>&1 &

done

# 等待所有节点启动完成
wait

echo "All nodes have been started."
