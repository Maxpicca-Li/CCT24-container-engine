#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syscall.h>
#include <errno.h>

#define STACK_SIZE (1024 * 1024)

// 变量: 需要设置的参数
char* root_fs = "/mnt/hgfs/hw2/rootfs";
char* container_name = "centos";
char* container_ip="192.168.1.2";
// 变量: 其他
const char* bridge = "br0";
const char* mygroup = "/sys/fs/cgroup/mygroup";
const char* cpu_time_cfg="50000 100000"; // 一个时间片 10w us; 任务只占据 5w us，即半个时间片
const char* mem_cfg="52428800"; // 任务只占据 50M 内存
char* command[] = {"/bin/bash", NULL};
char subgroup_path[256];

// 封装系统调用
int _system(const char* cmd){
    // printf("cmd: %s\n", cmd); // for debug
    return system(cmd);
}

// 封装 cgroup 参数更新
int _cgset(const char* subgroup_path, const char* resource, const char* set){
    char resource_path[256];
    snprintf(resource_path, sizeof(resource_path), "%s/%s", subgroup_path, resource);
    int fd = open(resource_path, O_WRONLY);
    if (fd == -1) {
        printf("ERR: open file %s failed!\n", resource_path);
        return -1;
    }
    if (write(fd, set, strlen(set)) == -1) {
        printf("ERR: write file %s failed!\n", resource_path);
        return -1;
    }
    return 0;
}

void set_env(){
    // 创建 bridge
    char cmd[20][100];
    int k=0;
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link add name %s type bridge", bridge);
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link set %s up", bridge);
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip addr add 192.168.1.1/24 dev %s", bridge);
    for(int i = 0; i < k; ++i) _system(cmd[i]);

    // 创建 mygroup
    mkdir(mygroup, 0755);
    _cgset(mygroup, "cgroup.subtree_control",  "+cpu +memory");
}

void clean_env(){
    // 删除 bridge
    char cmd[20][100];
    int k=0;
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link delete %s", bridge);
    for(int i = 0; i < k; ++i) _system(cmd[i]);

    // 删除 mygroup
    rmdir(mygroup);
}

void configure_child_cgroup(const char *container_name, pid_t pid) {
    // 在 cpu 子系统的 Hierarchy 下创建一个子目录，即生成一个对应的 cgroup，系统将自动为该子目录创建相关文件
    mkdir(subgroup_path, 0755);
    // 设置CPU时间片分配
    _cgset(subgroup_path, "cpu.max", cpu_time_cfg);
    // 设置内存分配
    _cgset(subgroup_path, "memory.max", mem_cfg);
    // 将 pid 放到该 group 中
    char buff[100];
    snprintf(buff, sizeof(buff), "%d", pid);
    _cgset(subgroup_path, "cgroup.procs", buff);
}

void configure_child_network(const char *container_name, pid_t pid) {
    char cmd[20][100];
    int k=0;
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link add %s_h type veth peer name eth0 netns %d", container_name, pid);
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link set %s_h master br0", container_name);
    snprintf(cmd[k++], sizeof(cmd[k]), "sudo ip link set %s_h up", container_name);
    for(int i = 0; i < k; ++i) _system(cmd[i]);
}

void clean_child(const char *container_name) {
    // veth-pair中的 veth_h 会随着 veth_c(eth0) 的结束而结束
    // 删除 subgroup
    rmdir(subgroup_path);
}

int child_func() {
    // 文件
    if (chroot(root_fs) != 0) {
        perror("chroot");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") != 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    // 进程
    if (mount("proc", "/proc", "proc", 0, "") != 0) {
        perror("mount proc");
        exit(EXIT_FAILURE);
    }

    // 系统信息
    if (sethostname(container_name, strlen(container_name)) < 0) {
        perror("set hostname");
        exit(EXIT_FAILURE);
    };

    // 用户信息：切换到root用户，因为设置子进程网络需要 root 权限
    if (setuid(0) != 0 || setgid(0) != 0) {
        perror("setuid/setgid");
        exit(EXIT_FAILURE);
    }

    // 网络信息：在子进程的网络空间中执行，但需要等父进程把 eth0 的 veth-pair 建立起来
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev eth0", container_ip);
    while(_system(cmd) != 0);
    _system("ip link set dev eth0 up");

    execvp(command[0], command);
    perror("execvp");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    // 参数设置
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <root_fs> <container_name> <container_ip>", argv[0]);
        return EXIT_FAILURE;
    }
    root_fs = argv[1];
    container_name = argv[2];
    container_ip = argv[3];
    snprintf(subgroup_path, sizeof(subgroup_path), "%s/%s", mygroup, container_name);
    
    // set_env(); // 改为 bash env.sh set，用于多容器创建

    // 子进程创建
    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    int flags = CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD;
    pid_t pid = clone(child_func, stack + STACK_SIZE, flags, command);
    if (pid == -1) {
        perror("clone");
        return EXIT_FAILURE;
    }
    
    // host 为 container 配置网络和控制组
    configure_child_network(container_name, pid);
    configure_child_cgroup(container_name, pid);

    waitpid(pid, NULL, 0);

    free(stack);
    clean_child(container_name);
    // clean_env(); // 改为 bash env.sh clean，用于多容器创建
    return 0;
}
