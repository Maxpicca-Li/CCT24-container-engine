# A Simple Container Engine

利用 Linux 操作系统的 Namespace 和 Cgroup 机制，实现的一个简单容器引擎，含以下功能：

* 实现进程、用户、文件系统、网络等方面的隔离
* 能够在 Ubuntu 系统上运行 CentOS 环境
* 能够实现同一操作系统下两个容器之间的网络通信
* 能够为容器分配定量的 CPU 和内存资源

## 环境说明

本次的操作环境如下：

* Ubuntu22.04, arm64 (in mac VMware machines)
* gcc
* ip
* cgroup2

**1、cgroup 版本检查**

`mount | grep cgroup`，如果输出包含 cgroup2，则表示系统正在使用 cgroup2。

**2、centos rootfs 下载**

* arm64: https://raw.githubusercontent.com/EXALAB/Anlinux-Resources/master/Rootfs/CentOS/arm64/centos-rootfs-arm64.tar.xz

* amd64: https://raw.githubusercontent.com/EXALAB/Anlinux-Resources/master/Rootfs/CentOS/amd64/centos-rootfs-amd64.tar.xz

也可以用其他途径下载，后解压到本地。

## 使用说明

### step1: 环境设置

运行 `env.sh`。

```shell
$ bash env.sh set  
net.ipv4.conf.all.forwarding = 1
+cpu +memory
ENV SET DONE.
```



### step2: 运行测试

#### 使用 C 程序

**编译**：

```shell
gcc container_engine.c -o container_engine
```

**运行** `Usage: ./container_engine <root_fs> <container_name> <container_ip> `：

```shell
$ sudo ./container_engine <your_rootfs_path> centos1 192.168.1.2
[root@centos1 /]# 
```



**测试：在容器内执行进程、用户、文件系统、网络等测试。**

```shell
[root@centos1 /]# ps -e
    PID TTY          TIME CMD
      1 ?        00:00:00 bash
     12 ?        00:00:00 ps
[root@centos1 /]# whoami
root
[root@centos1 /]# ls
bin  dev  etc  home  lib  lib64  lost+found  media  mnt  opt  proc  root  run  sbin  srv  sys  tmp  usr  var
[root@centos1 /]# ip a
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: eth0@if25: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 8e:a1:c9:77:d8:00 brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 192.168.1.2/24 scope global eth0
       valid_lft forever preferred_lft forever
    inet6 fe80::8ca1:c9ff:fe77:d800/64 scope link 
       valid_lft forever preferred_lft forever
[root@centos1 /]# ping 192.168.1.1
PING 192.168.1.1 (192.168.1.1) 56(84) bytes of data.
64 bytes from 192.168.1.1: icmp_seq=1 ttl=64 time=0.169 ms
64 bytes from 192.168.1.1: icmp_seq=2 ttl=64 time=0.045 ms
64 bytes from 192.168.1.1: icmp_seq=3 ttl=64 time=0.084 ms
^C
--- 192.168.1.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2031ms
rtt min/avg/max/mdev = 0.045/0.099/0.169/0.052 ms
```



**测试：容器资源定量分配。**

1. 先将 stress_test.sh 测试脚本复制到rootfs中。也可以用C写一个cpu、memory的压力程序进行测试，这里我下载的是最简rootfs，不含gcc，故使用 shell 脚本实现。

```shell
cp test/stress_test.sh <your_rootfs_path>/home/
```

2. 进入容器，进行压力测试

```shell
[root@centos1 /]# cd home
[root@centos1 home]# chmod +x ./stress_test.sh
[root@centos1 home]# ./stress_test.sh
请输入内存测试大小（MB）: 100
请输入测试持续时间（秒）: 20 
开始 CPU 压力测试...
开始内存压力测试...
结束压力测试...
压力测试完成！
Terminated
[root@centos1 home]# 
```

在主机上查看 `top`。测试结果如下：红色箭头所指的 bash 的CPU占比始终不超过50%，符合程序中 cgroup 的设置。mem 设置的上限为 50 MB（总内存为 4GB，占比上限为1.2%），图中结果亦符合。

![image-20240621223836828](https://gitee.com/Maxpicca/figure-bed/raw/master/img//image-20240621223836828.png)



**测试：两个容器之间通信连通性。**

在两个bash中分别执行以下命令：

```shell
sudo ./container_engine <your_rootfs_path> centos1 192.168.1.2
sudo ./container_engine <your_rootfs_path> centos2 192.168.1.3
```

测试如下：

![image-20240621215820849](https://gitee.com/Maxpicca/figure-bed/raw/master/img//image-20240621215820849.png)

#### 使用 shell 脚本

**运行** `Usage: container.sh <root_fs> <container_name> <container_ip>`

```shell
$ bash container.sh <your_rootfs_path> centos1 192.168.1.2
50000
52428800
[root@vm-ubuntu22 /]# 
```

如果需要 cgroup 进一步控制，则在`container.sh`执行后，在主机另一个 bash 中执行：

```shell
$ bash cgroup.sh set centos1
[sudo] password for maxpicca: 
50000 100000
52428800
878726
CGROUP SET DONE.
```

如果需要清除子进程控制组，则执行

```shell
$ bash cgroup.sh clean centos1
CGROUP CLEAN DONE.
```



**测试：**同上，不再赘述。

### step3: 环境清除

清除建立的网络桥和父控制组。

```shell
$ bash env.sh clean
ENV CLEAN DONE.
```



## 实现说明

整体逻辑上：

1. 利用 env.sh 创建网络桥、父控制组，并设置ipv4转发协议（用于多容器通信）。
2. 程序中，用 clone 创建子进程，进行细粒度隔离。
3. 子进程 clone 时，主进程利用子进程 pid 进行网络设置和子控制组设置。
4. 子进程结束后，清除主进程在上一步建立的网络link和子控制组。

> shell 脚本相对而言比C程序的逻辑更加明了，但实现上限于hub主水平有限，C程序和shell程序有一点不同，如：
>
> 1. C程序中，主机没有单独建立 netns，而是直接利用子程序clone时`CLONE_NEWNET`创建的新的命名空间，建立 veth 的容器端。而shell是主机单独建立一个 netns，再在这个 netns 内用 unshare 来执行 rootfs 的 /bin/bash；这种方式经过多次调试，发现无法直接在 C 中实现，故 C 中的实现有所简化。
> 2. C程序中，是直接通过 clone 子进程得到 pid，利用 pid 进行子控制组设置。但在 shell 程序中，无法直接获得 pid，所以是单独写了一个 `cgroup.sh` 用于绑定已经生成的子进程。



## 参考资料

1. [MiaoHao-oops/mycontainer](https://github.com/MiaoHao-oops/mycontainer)
2. [ChatGPT](https://chatgpt.com/)
