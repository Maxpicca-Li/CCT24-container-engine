#!/bin/bash

# 传递环境参数
if [ $# -lt 3 ]; then
    echo "Usage: $0 <root_fs> <container_name> <container_ip>"
    exit 1
fi
root_fs=$1
container_name=$2
container_ip=$3
# 建立网络空间
sudo ip netns add ${container_name}_net
sudo ip link add ${container_name}_h type veth peer name ${container_name}_c
sudo ip link set ${container_name}_c netns ${container_name}_net
sudo ip netns exec ${container_name}_net ip link set dev ${container_name}_c name eth0
sudo ip netns exec ${container_name}_net ip addr add $container_ip/24 dev eth0
sudo ip netns exec ${container_name}_net ip link set eth0 up
sudo ip link set ${container_name}_h master br0
sudo ip link set ${container_name}_h up
# 在建立的网络空间中执行 centos rootfs
sudo ip netns exec ${container_name}_net unshare --ipc --uts --mount --root $root_fs --pid --mount-proc --fork /bin/bash
# exit 后 delete 子网络和使用cgroup.sh后产生的subgroup
sudo ip link delete ${container_name}_h
sudo ip netns delete ${container_name}_net