#!/bin/bash

# 找到所有的 .proto 文件并编译
find src -type f -name "*.proto" | while read -r proto_file; do
  # 获取文件所在的目录
  dir=$(dirname "$proto_file")
  # 编译 .proto 文件并将生成的文件放在原目录下
  /home/fredyvia/code-v4.1-Release/OpenHarmony-v4.1-Release/OpenHarmony/out/rk3568/clang_x64/thirdparty/protobuf/protoc --experimental_allow_proto3_optional --proto_path=src --proto_path=/home/fredyvia/code-v4.1-Release/OpenHarmony-v4.1-Release/OpenHarmony/third_party/protobuf/src/ --proto_path=${dir} --cpp_out="src" "$proto_file"
  
  # 如果需要生成其他语言的代码，可以添加其他编译命令
  # protoc --proto_path="$dir" --java_out="$dir" "$proto_file"
  # protoc --proto_path="$dir" --python_out="$dir" "$proto_file"
  mv src/service.pb.h include/weight_raft
  generated_cc_file="src/$(basename "${proto_file}" .proto).pb.cc"
  if [[ -f "$generated_cc_file" ]]; then
    sed -i "s|#include \"$(basename "${proto_file}" .proto).pb.h\"|#include \"weight_raft/$(basename "${proto_file}" .proto).pb.h\"|g" "$generated_cc_file"
  fi

  echo "Compiled $proto_file and placed the output in $dir"
  # break
done

