#!/bin/bash

# 用户输入参数：内存大小测试（MB）、持续时间（秒）
read -p "请输入内存测试大小（MB）: " memory_size
read -p "请输入测试持续时间（秒）: " duration

# 生成 CPU 压力的函数
cpu_stress() {
    x=1
    while true
    do
        x=$x+1
    done
}

# 生成内存压力的函数
memory_stress() {
    mem_array=()
    for ((i=0; i<$(($memory_size * 1024 * 1024)); i++)); do
        mem_array+=($(mktemp -u XXXXXXXX))
    done
    sleep "$duration"
    unset mem_array
}

# 启动 CPU 压力测试
echo "开始 CPU 压力测试..."
cpu_stress &

# 启动内存压力测试
echo "开始内存压力测试..."
memory_stress &

# 等待指定时间
sleep "$duration"

# 结束 CPU 压力测试
echo "结束压力测试..."
echo "压力测试完成！"
pkill -f stress_test
