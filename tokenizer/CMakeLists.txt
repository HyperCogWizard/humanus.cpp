cmake_minimum_required(VERSION 3.10)
project(humanus_tokenizer)

# 复制测试数据到构建目录
file(COPY cl100k_base.tiktoken DESTINATION ${CMAKE_CURRENT_BINARY_DIR}) 