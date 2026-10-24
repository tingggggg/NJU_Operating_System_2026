#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <sys/syscall.h>

static double speedup = 10.0; // 10倍速度

static struct timeval real_start_tv;
static struct timeval virtual_start_tv;
static int initialized = 0;

static void init_time()
{
    if (!initialized) {
        // 使用 syscall 绕过 hook 获取真实时间，避免递归崩溃
        struct timespec ts;
        syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
        real_start_tv.tv_sec = ts.tv_sec;
        real_start_tv.tv_usec = ts.tv_nsec / 1000;
        virtual_start_tv = real_start_tv;
        initialized = 1;
    }
}

static void scale_timespec(const struct timespec *src, struct timespec *dst, double scale) {
    double ns = src->tv_sec * 1e9 + src->tv_nsec;
    ns /= scale;
    dst->tv_sec = (time_t)(ns / 1e9);
    dst->tv_nsec = (long)(ns - dst->tv_sec * 1e9);
    if (dst->tv_nsec < 0) { dst->tv_nsec = 0; }
}

static void scale_timeval(const struct timeval *src, struct timeval *dst, double scale) {
    double us = src->tv_sec * 1e6 + src->tv_usec;
    us /= scale;
    dst->tv_sec = (time_t)(us / 1e6);
    dst->tv_usec = (suseconds_t)(us - dst->tv_sec * 1e6);
    if (dst->tv_usec < 0) { dst->tv_usec = 0; }
}

// gettimeofday 覆盖
int gettimeofday(struct timeval *__restrict tv, void *__restrict tz)
{
    static int (*real_gettimeofday)(struct timeval *, struct timezone *) = NULL;
    if (!real_gettimeofday) {
        real_gettimeofday = dlsym(RTLD_NEXT, "gettimeofday");
    }

    init_time();

    struct timeval now;
    real_gettimeofday(&now, tz);  // 获取真实当前时间

    // 计算速度提升后的虚拟时间: 虚拟 = 虚拟起点 + (真实经过 * speedup)
    double real_elapsed =
        (now.tv_sec - real_start_tv.tv_sec) +
        (now.tv_usec - real_start_tv.tv_usec)/1000000.0;

    double virtual_elapsed = real_elapsed * speedup;

    time_t vsec = virtual_start_tv.tv_sec + (time_t)virtual_elapsed;
    suseconds_t vusec =
        virtual_start_tv.tv_usec +
        (suseconds_t)((virtual_elapsed - (time_t)virtual_elapsed)*1000000);

    // 微秒超出1秒要进位
    if (vusec >= 1000000) {
        vsec += vusec / 1000000;
        vusec = vusec % 1000000;
    }

    if (tv) {
        tv->tv_sec = vsec;
        tv->tv_usec = vusec;
    }

    return 0;
}

// clock_gettime 覆盖
int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    // 使用 int 代替 enum 做状态机，防止 dlsym 期间递归
    static int state = 0; // 0=未解析, 1=解析中, 2=已解析
    static int (*real_clock_gettime)(clockid_t, struct timespec *) = NULL;

    if (state == 1) {
        // dlsym 期间递归调用，直接用 syscall
        return syscall(SYS_clock_gettime, clk_id, tp);
    }

    if (!real_clock_gettime) {
        state = 1;
        real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
        state = 2;
        if (!real_clock_gettime) return -1;
    }

    struct timespec now;
    int ret = real_clock_gettime(clk_id, &now); // 获取真实时间
    if (ret != 0)
        return ret;

    if (clk_id == CLOCK_REALTIME || clk_id == CLOCK_MONOTONIC) {
        struct timeval now_tv;
        now_tv.tv_sec = now.tv_sec;
        now_tv.tv_usec = now.tv_nsec / 1000;

        init_time();

        double real_elapsed =
            (now_tv.tv_sec - real_start_tv.tv_sec) +
            (now_tv.tv_usec - real_start_tv.tv_usec)/1000000.0;

        double virtual_elapsed = real_elapsed * speedup;

        time_t vsec = virtual_start_tv.tv_sec + (time_t)virtual_elapsed;
        long vnsec = virtual_start_tv.tv_usec * 1000 +
                    (long)((virtual_elapsed - (time_t)virtual_elapsed)*1e9);

        if (vnsec >= 1e9) {
            vsec += vnsec / (long)1e9;
            vnsec = vnsec % (long)1e9;
        }
        if (tp) {
            tp->tv_sec = vsec;
            tp->tv_nsec = vnsec;
        }
        return 0;
    } else {
        // 其他 clock 通常原样返回
        if (tp)
            *tp = now;
        return ret;
    }
}

// usleep 覆盖
int usleep(useconds_t usec)
{
    static int (*real_usleep)(useconds_t) = NULL;
    if (!real_usleep) {
        real_usleep = dlsym(RTLD_NEXT, "usleep");
    }
    // 用更短的真实usleep来模拟虚拟加速
    useconds_t real_usec = (useconds_t)((double)usec / speedup);
    return real_usleep(real_usec);
}

// alarm 覆盖
unsigned int alarm(unsigned int seconds)
{
    static unsigned int (*real_alarm)(unsigned int) = NULL;
    if (!real_alarm) {
        real_alarm = dlsym(RTLD_NEXT, "alarm");
    }
    // 加速后，真实alarm时间应缩短
    unsigned int real_seconds = (unsigned int)((double)seconds / speedup);
    return real_alarm(real_seconds);
}

// nanosleep 覆盖
int nanosleep(const struct timespec *req, struct timespec *rem)
{
    static int (*real_nanosleep)(const struct timespec *, struct timespec *) = NULL;
    if (!real_nanosleep) {
        real_nanosleep = dlsym(RTLD_NEXT, "nanosleep");
    }
    struct timespec new_req;
    scale_timespec(req, &new_req, speedup);
    return real_nanosleep(&new_req, rem);
}

// sleep 覆盖
unsigned int sleep(unsigned int seconds)
{
    static unsigned int (*real_sleep)(unsigned int) = NULL;
    if (!real_sleep) {
        real_sleep = dlsym(RTLD_NEXT, "sleep");
    }
    unsigned int real_seconds = (unsigned int)((double)seconds / speedup);
    return real_sleep(real_seconds);
}

// time 覆盖
time_t time(time_t *tloc)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tloc)
        *tloc = tv.tv_sec;
    return tv.tv_sec;
}

// select 覆盖
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout)
{
    static int (*real_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *) = NULL;
    if (!real_select)
        real_select = dlsym(RTLD_NEXT, "select");

    if (timeout) {
        struct timeval scaled;
        scale_timeval(timeout, &scaled, speedup);
        return real_select(nfds, readfds, writefds, exceptfds, &scaled);
    } else {
        return real_select(nfds, readfds, writefds, exceptfds, NULL);
    }
}

// pselect 覆盖
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask)
{
    static int (*real_pselect)(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *) = NULL;
    if (!real_pselect)
        real_pselect = dlsym(RTLD_NEXT, "pselect");

    if (timeout) {
        struct timespec scaled;
        scale_timespec(timeout, &scaled, speedup);
        return real_pselect(nfds, readfds, writefds, exceptfds, &scaled, sigmask);
    } else {
        return real_pselect(nfds, readfds, writefds, exceptfds, NULL, sigmask);
    }
}

// poll 覆盖
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    static int (*real_poll)(struct pollfd *, nfds_t, int) = NULL;
    if (!real_poll)
        real_poll = dlsym(RTLD_NEXT, "poll");
    int tmo = timeout;
    if (timeout > 0) {
        tmo = (int)((double)timeout / speedup);
        if (tmo == 0)
            tmo = 1;
    }
    return real_poll(fds, nfds, tmo);
}

// ppoll 覆盖
int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *tmo_p, const sigset_t *sigmask)
{
    static int (*real_ppoll)(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *) = NULL;
    if (!real_ppoll)
        real_ppoll = dlsym(RTLD_NEXT, "ppoll");
    if (tmo_p) {
        struct timespec scaled;
        scale_timespec(tmo_p, &scaled, speedup);
        return real_ppoll(fds, nfds, &scaled, sigmask);
    }
    return real_ppoll(fds, nfds, NULL, sigmask);
}

// timerfd_settime 覆盖
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
{
    static int (*real_timerfd_settime)(int, int, const struct itimerspec *, struct itimerspec *) = NULL;
    if (!real_timerfd_settime)
        real_timerfd_settime = dlsym(RTLD_NEXT, "timerfd_settime");
    if (new_value) {
        struct itimerspec scaled = *new_value;
        scale_timespec(&new_value->it_value, &scaled.it_value, speedup);
        scale_timespec(&new_value->it_interval, &scaled.it_interval, speedup);
        return real_timerfd_settime(fd, flags, &scaled, old_value);
    }
    return real_timerfd_settime(fd, flags, new_value, old_value);
}

// setitimer 覆盖
int setitimer(__itimer_which_t which, const struct itimerval *new_value, struct itimerval *old_value)
{
    static int (*real_setitimer)(__itimer_which_t, const struct itimerval *, struct itimerval *) = NULL;
    if (!real_setitimer)
        real_setitimer = dlsym(RTLD_NEXT, "setitimer");
    if (new_value) {
        struct itimerval scaled = *new_value;
        scale_timeval(&new_value->it_value, &scaled.it_value, speedup);
        scale_timeval(&new_value->it_interval, &scaled.it_interval, speedup);
        return real_setitimer(which, &scaled, old_value);
    }
    return real_setitimer(which, new_value, old_value);
}

// utimes 覆盖
int utimes(const char *filename, const struct timeval times[2])
{
    static int (*real_utimes)(const char *, const struct timeval[2]) = NULL;
    if (!real_utimes)
        real_utimes = dlsym(RTLD_NEXT, "utimes");
    if (times) {
        struct timeval vt[2];
        scale_timeval(&times[0], &vt[0], 1.0/speedup); // 恢复到真实时间
        scale_timeval(&times[1], &vt[1], 1.0/speedup);
        return real_utimes(filename, vt);
    }
    return real_utimes(filename, times);
}
