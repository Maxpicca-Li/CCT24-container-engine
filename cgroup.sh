#!/bin/bash

# cgroup v2 没有找到如何直接新建进程
# 故这里单独用一个脚本对已知容器pid分配cgroup

# 传递环境参数
if [ $# -lt 2 ]; then
    echo "Usage: $0 <option: set/delete> <container_name>"
    exit 1
fi
option=$1
container_name=$2
subgroup=/sys/fs/cgroup/mygroup/${container_name}
case $option in
    "set")
        # 创建subgroup并设置资源限制
        # 建立控制组
        sudo mkdir $subgroup
        echo "50000 100000" | sudo tee ${subgroup}/cpu.max
        echo "52428800" | sudo tee ${subgroup}/memory.max
        # 将进程加入到 cgroup 
        pid=$(pgrep -u root bash)
        echo "$pid" | sudo tee ${subgroup}/cgroup.procs
        echo "CGROUP SET DONE."
        ;;
    "clean")
        # 先处理 cgroup 中可能存在的进程
        for pid in $(cat $subgroup/cgroup.procs); do 
            # echo $pid > /sys/fs/cgroup/cgroup.procs # 移动到默认空间
            sudo kill -9 $pid # 直接杀死
        done
        # 删除 subgroup
        sudo rmdir ${subgroup}
        echo "CGROUP CLEAN DONE."
        ;;
    *)
        echo "Invalid option. Please choose "set" or "clean"."
        exit 1
        ;;
esac
