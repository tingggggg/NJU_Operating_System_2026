// Author: doubao-seed-2.0-code
// 讲真，很强，但完全相同的 prompt，在 Claude Opus 4.6 面前就像个弱智 😭

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define BUFFER_SIZE 4096

void read_proc_file(const char* filename, const char* description) {
    char path[64];
    char buffer[BUFFER_SIZE];
    int fd;
    ssize_t bytesRead;

    snprintf(path, sizeof(path), "/proc/self/%s", filename);

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    if (bytesRead < 0) {
        perror("read");
        close(fd);
        return;
    }

    buffer[bytesRead] = '\0';

    printf("\n=== %s ===\n", description);
    printf("%s", buffer);

    close(fd);
}

void display_file_descriptors() {
    char path[] = "/proc/self/fd";
    struct dirent* entry;
    DIR* dir;

    printf("\n=== 打开的文件描述符 ===\n");

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char fd_path[64];
        char link_path[256];
        ssize_t link_len;

        snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%s", entry->d_name);
        link_len = readlink(fd_path, link_path, sizeof(link_path) - 1);

        if (link_len != -1) {
            link_path[link_len] = '\0';
            printf("fd %s -> %s\n", entry->d_name, link_path);
        }
    }

    closedir(dir);
}

void display_rusage_info() {
    struct rusage usage;
    printf("\n=== 资源使用统计 ===\n");

    if (getrusage(RUSAGE_SELF, &usage) == -1) {
        perror("getrusage");
        return;
    }

    printf("用户 CPU 时间: %ld.%06ld 秒\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("系统 CPU 时间: %ld.%06ld 秒\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
    printf("最大常驻集大小: %ld KB\n", usage.ru_maxrss);
    printf("缺页错误数（无 IO）: %ld\n", usage.ru_minflt);
    printf("缺页错误数（有 IO）: %ld\n", usage.ru_majflt);
    printf("块输入操作数: %ld\n", usage.ru_inblock);
    printf("块输出操作数: %ld\n", usage.ru_oublock);
    printf("进程切换次数: %ld\n", usage.ru_nivcsw + usage.ru_nvcsw);
    printf("自愿上下文切换: %ld\n", usage.ru_nvcsw);
    printf("非自愿上下文切换: %ld\n", usage.ru_nivcsw);
    printf("信号接收数: %ld\n", usage.ru_nsignals);
}

void display_sysinfo_info() {
    struct sysinfo info;
    printf("\n=== 系统信息 ===\n");

    if (sysinfo(&info) == -1) {
        perror("sysinfo");
        return;
    }

    printf("系统启动时间: %ld 秒\n", info.uptime);
    printf("总 RAM: %lu KB\n", info.totalram * info.mem_unit / 1024);
    printf("可用 RAM: %lu KB\n", info.freeram * info.mem_unit / 1024);
    printf("共享 RAM: %lu KB\n", info.sharedram * info.mem_unit / 1024);
    printf("缓冲区 RAM: %lu KB\n", info.bufferram * info.mem_unit / 1024);
    printf("总交换空间: %lu KB\n", info.totalswap * info.mem_unit / 1024);
    printf("可用交换空间: %lu KB\n", info.freeswap * info.mem_unit / 1024);
    printf("进程数: %d\n", info.procs);
}

void display_scheduling_info() {
    int policy;
    struct sched_param param;
    int priority;

    printf("\n=== 调度信息 ===\n");

    policy = sched_getscheduler(getpid());
    if (policy == -1) {
        perror("sched_getscheduler");
        return;
    }

    printf("调度策略: ");
    switch (policy) {
        case SCHED_FIFO:
            printf("SCHED_FIFO (实时 FIFO)\n");
            break;
        case SCHED_RR:
            printf("SCHED_RR (实时轮询)\n");
            break;
        case SCHED_OTHER:
            printf("SCHED_OTHER (默认分时)\n");
            break;
        default:
            printf("未知 (%d)\n", policy);
    }

    if (sched_getparam(getpid(), &param) == -1) {
        perror("sched_getparam");
        return;
    }

    printf("调度优先级: %d\n", param.sched_priority);

    priority = getpriority(PRIO_PROCESS, getpid());
    if (priority == -1 && errno != 0) {
        perror("getpriority");
    } else {
        printf("nice 值: %d\n", priority);
    }
}

void display_resource_limits() {
    struct rlimit rlim;
    printf("\n=== 资源限制 ===\n");

    if (getrlimit(RLIMIT_CPU, &rlim) == 0) {
        printf("CPU 时间限制: %lu / %lu 秒\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_FSIZE, &rlim) == 0) {
        printf("文件大小限制: %lu / %lu 字节\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_DATA, &rlim) == 0) {
        printf("数据段大小限制: %lu / %lu 字节\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_STACK, &rlim) == 0) {
        printf("栈大小限制: %lu / %lu 字节\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        printf("核心转储大小限制: %lu / %lu 字节\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_RSS, &rlim) == 0) {
        printf("最大常驻集大小: %lu / %lu 页\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        printf("文件描述符数限制: %lu / %lu\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }

    if (getrlimit(RLIMIT_AS, &rlim) == 0) {
        printf("地址空间大小限制: %lu / %lu 字节\n", (unsigned long)rlim.rlim_cur, (unsigned long)rlim.rlim_max);
    }
}

void display_clock_info() {
    struct timespec real_time, cpu_time, monotonic_time;

    printf("\n=== 时钟信息 ===\n");

    if (clock_gettime(CLOCK_REALTIME, &real_time) == 0) {
        printf("实时时钟: %ld.%09ld 秒\n", real_time.tv_sec, real_time.tv_nsec);
    }

    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_time) == 0) {
        printf("进程 CPU 时间: %ld.%09ld 秒\n", cpu_time.tv_sec, cpu_time.tv_nsec);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &monotonic_time) == 0) {
        printf("单调时钟: %ld.%09ld 秒\n", monotonic_time.tv_sec, monotonic_time.tv_nsec);
    }
}

int main() {
    printf("=== Linux 进程操作系统状态查询工具 ===\n");
    printf("PID: %d\n", getpid());
    printf("PPID: %d\n", getppid());
    printf("UID: %d\n", getuid());
    printf("GID: %d\n", getgid());

    // 系统调用获取的信息
    display_rusage_info();
    display_sysinfo_info();
    display_scheduling_info();
    display_resource_limits();
    display_clock_info();

    // procfs 获取的信息
    read_proc_file("status", "进程状态信息");
    read_proc_file("stat", "进程统计信息");
    read_proc_file("maps", "内存映射信息");
    read_proc_file("statm", "内存使用统计");

    display_file_descriptors();

    read_proc_file("environ", "环境变量");
    read_proc_file("cmdline", "命令行参数");

    printf("\n=== 线程信息 ===\n");
    char task_path[] = "/proc/self/task";
    struct dirent* entry;
    DIR* dir;

    dir = opendir(task_path);
    if (dir == NULL) {
        perror("opendir");
    } else {
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            printf("线程 ID: %s\n", entry->d_name);
        }
        closedir(dir);
    }

    return 0;
}
