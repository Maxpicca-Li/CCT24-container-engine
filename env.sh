#!/bin/bash

# 传递环境参数
if [ $# -lt 1 ]; then
    echo "Usage: $0 <option: set/clean>"
    exit 1
fi

case $1 in
    "set")
        # 设置转发规则，用于同一操作系统下两个容器之间基于 ipv4 转发表进行网络通信
        sudo sysctl net.ipv4.conf.all.forwarding=1
        sudo iptables -P FORWARD ACCEPT
        # 创建 bridge，用于主机和容器通信
        sudo ip link add name br0 type bridge
        sudo ip link set br0 up
        sudo ip addr add 192.168.1.1/24 dev br0
        # 创建 mygroup，用于管理容器资源
        sudo mkdir /sys/fs/cgroup/mygroup
        echo "+cpu +memory" | sudo tee /sys/fs/cgroup/mygroup/cgroup.subtree_control
        # 结束
        echo "ENV SET DONE."
        ;;
    "clean")
        # 删除 bridge
        sudo ip link delete br0
        # 删除 mygroup
        sudo rmdir /sys/fs/cgroup/mygroup/
        # 结束
        echo "ENV CLEAN DONE."
        ;;
    *)
        echo "Invalid option. Please choose "set" or "clean"."
        exit 1
        ;;
esac
