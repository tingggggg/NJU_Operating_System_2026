// Author: Claude Opus 4.6
// 不知道说什么好 (绿导师该退休了)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sched.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/personality.h>
#include <linux/capability.h>
#include <linux/ioprio.h>

static void print_section(const char *title) {
    printf("\n\033[1;36m=== %s ===\033[0m\n", title);
}

static void dump_file(const char *path) {
    char buf[4096];
    int fd = open(path, O_RDONLY);
    if (fd < 0) { printf("  (cannot read %s)\n", path); return; }
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    close(fd);
}

static void dump_proc(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/%s", name);
    printf("\n\033[1;33m[%s]\033[0m\n", name);
    dump_file(path);
}

static void show_basic_ids(void) {
    print_section("Process Identity");
    printf("  PID:  %d\n", getpid());
    printf("  PPID: %d\n", getppid());
    printf("  PGID: %d\n", getpgrp());
    printf("  SID:  %d\n", getsid(0));
    printf("  UID:  %d  EUID: %d\n", getuid(), geteuid());
    printf("  GID:  %d  EGID: %d\n", getgid(), getegid());
}

static void show_cmdline(void) {
    print_section("Command Line");
    char buf[4096];
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    printf("  ");
    for (ssize_t i = 0; i < n; i++)
        putchar(buf[i] ? buf[i] : ' ');
    putchar('\n');
}

static void show_exe(void) {
    char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; printf("  exe -> %s\n", buf); }
}

static void show_cwd_root(void) {
    char buf[1024];
    ssize_t n;
    n = readlink("/proc/self/cwd", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; printf("  cwd  -> %s\n", buf); }
    n = readlink("/proc/self/root", buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; printf("  root -> %s\n", buf); }
}

static void show_status(void) {
    print_section("Process Status (/proc/self/status)");
    dump_file("/proc/self/status");
}

static void show_stat(void) {
    print_section("Scheduling Info (/proc/self/stat)");
    dump_file("/proc/self/stat");
    putchar('\n');
}

static void show_maps(void) {
    print_section("Memory Mappings (/proc/self/maps)");
    dump_file("/proc/self/maps");
}

static void show_fds(void) {
    print_section("Open File Descriptors");
    DIR *d = opendir("/proc/self/fd");
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        char path[256], target[1024];
        snprintf(path, sizeof(path), "/proc/self/fd/%s", ent->d_name);
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n > 0) { target[n] = '\0'; printf("  fd %3s -> %s\n", ent->d_name, target); }
    }
    closedir(d);
}

static void show_limits(void) {
    print_section("Resource Limits (/proc/self/limits)");
    dump_file("/proc/self/limits");
}

static void show_sched(void) {
    print_section("Scheduler");
    int policy = sched_getscheduler(0);
    const char *names[] = { "SCHED_OTHER", "SCHED_FIFO", "SCHED_RR" };
    printf("  policy: %s (%d)\n", (policy >= 0 && policy <= 2) ? names[policy] : "unknown", policy);
    struct sched_param sp;
    if (sched_getparam(0, &sp) == 0)
        printf("  priority: %d\n", sp.sched_priority);
    cpu_set_t cpuset;
    if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        printf("  cpus allowed:");
        for (int i = 0; i < CPU_SETSIZE; i++)
            if (CPU_ISSET(i, &cpuset)) printf(" %d", i);
        putchar('\n');
    }
}

static void show_ns(void) {
    print_section("Namespaces (/proc/self/ns)");
    DIR *d = opendir("/proc/self/ns");
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        char path[256], target[256];
        snprintf(path, sizeof(path), "/proc/self/ns/%s", ent->d_name);
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n > 0) { target[n] = '\0'; printf("  %-12s -> %s\n", ent->d_name, target); }
    }
    closedir(d);
}

static void show_cgroups(void) {
    print_section("Cgroups (/proc/self/cgroup)");
    dump_file("/proc/self/cgroup");
}

static void show_mounts(void) {
    print_section("Mount Info (/proc/self/mountinfo) [first 20 lines]");
    char buf[8192];
    int fd = open("/proc/self/mountinfo", O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';
    int lines = 0;
    for (char *p = buf; *p && lines < 20; p++)
        if (*p == '\n') lines++;
    char *end = buf;
    lines = 0;
    while (*end && lines < 20) { if (*end == '\n') lines++; end++; }
    *end = '\0';
    printf("%s\n", buf);
}

static void show_environ_summary(void) {
    print_section("Environment Variables (count)");
    int count = 0;
    int fd = open("/proc/self/environ", O_RDONLY);
    if (fd < 0) return;
    char buf[65536];
    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    for (ssize_t i = 0; i < n; i++)
        if (buf[i] == '\0') count++;
    printf("  %d environment variables\n", count);
}

static void show_io(void) {
    print_section("I/O Statistics (/proc/self/io)");
    dump_file("/proc/self/io");
}

static void show_rusage(void) {
    print_section("Resource Usage (getrusage)");
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) {
        printf("  user time:   %ld.%06lds\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec);
        printf("  system time: %ld.%06lds\n", ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        printf("  max RSS:     %ld KB\n", ru.ru_maxrss);
        printf("  page faults: %ld (minor) %ld (major)\n", ru.ru_minflt, ru.ru_majflt);
        printf("  vol ctx sw:  %ld\n", ru.ru_nvcsw);
        printf("  invol ctx sw:%ld\n", ru.ru_nivcsw);
    }
}

/* ---- Below: info NOT available via procfs ---- */

static void show_umask(void) {
    print_section("File Creation Mask (umask syscall)");
    mode_t m = umask(0);
    umask(m);
    printf("  umask: %04o\n", m);
}

static void show_groups(void) {
    print_section("Supplementary Groups (getgroups)");
    gid_t groups[128];
    int n = getgroups(128, groups);
    if (n > 0) {
        printf("  %d groups:", n);
        for (int i = 0; i < n; i++) printf(" %d", groups[i]);
        putchar('\n');
    }
}

static void show_prctl(void) {
    print_section("prctl Attributes");

    char name[17] = {0};
    if (prctl(PR_GET_NAME, name) == 0)
        printf("  thread name:    %s\n", name);

    int dumpable = prctl(PR_GET_DUMPABLE);
    printf("  dumpable:       %d (%s)\n", dumpable,
           dumpable == 1 ? "SUID_DUMP_USER" : dumpable == 2 ? "SUID_DUMP_ROOT" : "not dumpable");

    int seccomp = prctl(PR_GET_SECCOMP);
    printf("  seccomp mode:   %d (%s)\n", seccomp,
           seccomp == 0 ? "disabled" : seccomp == 1 ? "strict" : "filter");

    int no_new_privs = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
    printf("  no_new_privs:   %d\n", no_new_privs);

    int tsc = prctl(PR_GET_TSC);
    if (tsc >= 0)
        printf("  TSC access:     %s\n", tsc == PR_TSC_ENABLE ? "enabled" : "disabled");

    unsigned long thp;
    if (prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0) >= 0)
        printf("  THP disable:    %d\n", prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0));

    int timerslack = prctl(PR_GET_TIMERSLACK);
    if (timerslack > 0)
        printf("  timer slack:    %d ns\n", timerslack);

    int child_sub = prctl(PR_GET_CHILD_SUBREAPER);
    if (child_sub >= 0)
        printf("  child subreaper:%d\n", child_sub);
}

static void show_personality(void) {
    print_section("Execution Domain (personality)");
    unsigned long p = personality(0xffffffff);
    printf("  personality:    0x%lx", p);
    if (p == 0) printf(" (PER_LINUX)");
    if (p & 0x0008000) printf(" ADDR_NO_RANDOMIZE");
    if (p & 0x0000001) printf(" STICKY_TIMEOUTS");
    if (p & 0x0000002) printf(" WHOLE_SECONDS");
    if (p & 0x0200000) printf(" READ_IMPLIES_EXEC");
    if (p & 0x0400000) printf(" ADDR_LIMIT_32BIT");
    if (p & 0x0800000) printf(" SHORT_INODE");
    putchar('\n');
}

static void show_capabilities(void) {
    print_section("Capabilities (capget syscall)");
    struct __user_cap_header_struct hdr = { _LINUX_CAPABILITY_VERSION_3, 0 };
    struct __user_cap_data_struct data[2];
    if (syscall(SYS_capget, &hdr, data) == 0) {
        unsigned long long eff = (unsigned long long)data[1].effective << 32 | data[0].effective;
        unsigned long long perm = (unsigned long long)data[1].permitted << 32 | data[0].permitted;
        unsigned long long inh = (unsigned long long)data[1].inheritable << 32 | data[0].inheritable;
        printf("  effective:   0x%016llx\n", eff);
        printf("  permitted:   0x%016llx\n", perm);
        printf("  inheritable: 0x%016llx\n", inh);
    }
}

static void show_signals(void) {
    print_section("Signal State (sigprocmask/sigpending)");
    sigset_t blocked, pending;
    sigemptyset(&blocked);
    sigemptyset(&pending);
    sigprocmask(SIG_BLOCK, NULL, &blocked);
    sigpending(&pending);
    printf("  blocked signals: ");
    int any = 0;
    for (int s = 1; s < 64; s++)
        if (sigismember(&blocked, s)) { printf("%d ", s); any = 1; }
    if (!any) printf("(none)");
    printf("\n  pending signals: ");
    any = 0;
    for (int s = 1; s < 64; s++)
        if (sigismember(&pending, s)) { printf("%d ", s); any = 1; }
    if (!any) printf("(none)");
    putchar('\n');

    printf("  signal dispositions:\n");
    for (int s = 1; s < 32; s++) {
        struct sigaction sa;
        if (sigaction(s, NULL, &sa) == 0 && sa.sa_handler != SIG_DFL) {
            printf("    sig %2d: %s\n", s,
                   sa.sa_handler == SIG_IGN ? "SIG_IGN" : "handler installed");
        }
    }
}

static void show_itimers(void) {
    print_section("Interval Timers (getitimer)");
    const char *names[] = { "REAL", "VIRTUAL", "PROF" };
    for (int w = 0; w < 3; w++) {
        struct itimerval it;
        if (getitimer(w, &it) == 0 && (it.it_value.tv_sec || it.it_value.tv_usec)) {
            printf("  ITIMER_%-8s value=%ld.%06ld interval=%ld.%06ld\n",
                   names[w], it.it_value.tv_sec, it.it_value.tv_usec,
                   it.it_interval.tv_sec, it.it_interval.tv_usec);
        } else {
            printf("  ITIMER_%-8s (inactive)\n", names[w]);
        }
    }
}

static void show_clocks(void) {
    print_section("Process Clocks (clock_gettime)");
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0)
        printf("  CLOCK_PROCESS_CPUTIME: %ld.%09lds\n", ts.tv_sec, ts.tv_nsec);
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
        printf("  CLOCK_THREAD_CPUTIME:  %ld.%09lds\n", ts.tv_sec, ts.tv_nsec);
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        printf("  CLOCK_MONOTONIC:       %ld.%09lds\n", ts.tv_sec, ts.tv_nsec);
    if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0)
        printf("  CLOCK_BOOTTIME:        %ld.%09lds\n", ts.tv_sec, ts.tv_nsec);
}

static void show_ioprio(void) {
    print_section("I/O Priority (ioprio_get syscall)");
    int ioprio = syscall(SYS_ioprio_get, 1 /* IOPRIO_WHO_PROCESS */, 0);
    if (ioprio >= 0) {
        int class = ioprio >> 13;
        int data = ioprio & 0x1fff;
        const char *cnames[] = { "NONE", "RT", "BE", "IDLE" };
        printf("  class: %s (%d)  level: %d\n",
               class <= 3 ? cnames[class] : "unknown", class, data);
    }
}

static void show_tid(void) {
    print_section("Thread ID (gettid syscall)");
    printf("  TID: %ld\n", syscall(SYS_gettid));
}

static void show_brk(void) {
    print_section("Program Break (brk syscall)");
    void *brk = sbrk(0);
    printf("  current brk: %p\n", brk);
}

static void show_uname(void) {
    print_section("Kernel Info (uname)");
    struct utsname u;
    if (uname(&u) == 0) {
        printf("  sysname:  %s\n", u.sysname);
        printf("  nodename: %s\n", u.nodename);
        printf("  release:  %s\n", u.release);
        printf("  version:  %s\n", u.version);
        printf("  machine:  %s\n", u.machine);
    }
}

int main(int argc, char *argv[]) {
    printf("\033[1;32m*** proc-info: OS-level process state viewer ***\033[0m\n");

    show_basic_ids();
    show_cmdline();
    show_exe();
    show_cwd_root();
    show_status();
    show_stat();
    show_sched();
    show_fds();
    show_maps();
    show_limits();
    show_ns();
    show_cgroups();
    show_mounts();
    show_environ_summary();
    show_io();
    show_rusage();
    show_umask();
    show_groups();
    show_prctl();
    show_personality();
    show_capabilities();
    show_signals();
    show_itimers();
    show_clocks();
    show_ioprio();
    show_tid();
    show_brk();
    show_uname();

    return 0;
}
