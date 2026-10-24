#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static struct termios g_orig_termios;
static int g_in_fd = STDIN_FILENO;
static int g_out_fd = STDOUT_FILENO;

static volatile sig_atomic_t g_got_sigchld = 0;

static void on_sigchld(int sig) {
    (void)sig;
    g_got_sigchld = 1;
}

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

static void write_all(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            die("write");
        }
        p += (size_t)w;
        n -= (size_t)w;
    }
}

static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) die("clock_gettime");
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static void term_restore(void) {
    (void)tcsetattr(g_in_fd, TCSANOW, &g_orig_termios);
    // 恢复光标，清理屏幕
    const char *seq = "\x1b[?25h\x1b[0m\x1b[2J\x1b[H";
    write_all(g_out_fd, seq, strlen(seq));
}

static void term_raw(void) {
    if (tcgetattr(g_in_fd, &g_orig_termios) != 0) die("tcgetattr");
    atexit(term_restore);

    struct termios raw = g_orig_termios;
    // 使用简化版 raw，避免依赖 cfmakeraw
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    // 不要让 Ctrl-C 直接杀掉本程序，把 0x03 转发给 bash
    raw.c_lflag &= ~(ISIG);

    if (tcsetattr(g_in_fd, TCSANOW, &raw) != 0) die("tcsetattr");
}

static int pty_master_open(char *slave_name, size_t slave_name_sz) {
    // 不使用 O_CLOEXEC 以避免移植性问题
    int mfd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
    if (mfd < 0) die("open(/dev/ptmx)");

    // 手动设置 FD_CLOEXEC
    int fl = fcntl(mfd, F_GETFD);
    if (fl != -1) {
        (void)fcntl(mfd, F_SETFD, fl | FD_CLOEXEC);
    }

    if (grantpt(mfd) != 0) die("grantpt");
    if (unlockpt(mfd) != 0) die("unlockpt");

    // 使用 ptsname 代替 ptsname_r，避免移植性问题
    char *name = ptsname(mfd);
    if (name == NULL) die("ptsname");
    if (strlen(name) + 1 > slave_name_sz) {
        errno = ERANGE;
        die("slave name buffer too small");
    }
    strncpy(slave_name, name, slave_name_sz);
    slave_name[slave_name_sz - 1] = '\0';

    return mfd;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buf_t;

static void buf_reserve(buf_t *b, size_t need) {
    if (b->cap >= need) return;
    size_t ncap = b->cap ? b->cap : 4096;
    while (ncap < need) ncap *= 2;
    char *p = (char *)realloc(b->data, ncap);
    if (!p) die("realloc");
    b->data = p;
    b->cap = ncap;
}

static void buf_append(buf_t *b, const void *p, size_t n) {
    buf_reserve(b, b->len + n);
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static void buf_consume(buf_t *b, size_t n) {
    if (n >= b->len) {
        b->len = 0;
        return;
    }
    memmove(b->data, b->data + n, b->len - n);
    b->len -= n;
}

typedef struct {
    int inner_rows, inner_cols;
    int top, left;   // 1-based for ANSI cursor positioning
    int cur_r, cur_c;
    char *cells;     // inner_rows * inner_cols
} view_t;

static void ansi_move(int r, int c) {
    char seq[64];
    int n = snprintf(seq, sizeof(seq), "\x1b[%d;%dH", r, c);
    write_all(g_out_fd, seq, (size_t)n);
}

static void view_scroll(view_t *v) {
    if (v->inner_rows <= 1) return;
    size_t rowbytes = (size_t)v->inner_cols;
    memmove(v->cells, v->cells + rowbytes, rowbytes * (size_t)(v->inner_rows - 1));
    memset(v->cells + rowbytes * (size_t)(v->inner_rows - 1), ' ', rowbytes);
    v->cur_r = v->inner_rows - 1;
    if (v->cur_r < 0) v->cur_r = 0;
}

static void view_newline(view_t *v) {
    v->cur_c = 0;
    v->cur_r++;
    if (v->cur_r >= v->inner_rows) view_scroll(v);
}

static void view_put_char(view_t *v, char ch) {
    if (v->cur_r < 0) v->cur_r = 0;
    if (v->cur_c < 0) v->cur_c = 0;

    if (v->cur_c >= v->inner_cols) view_newline(v);
    if (v->cur_r >= v->inner_rows) view_scroll(v);

    v->cells[v->cur_r * v->inner_cols + v->cur_c] = ch;
    v->cur_c++;
    if (v->cur_c >= v->inner_cols) view_newline(v);
}

static void view_put_caret(view_t *v, unsigned char b) {
    if (b == 0x7f) { view_put_char(v, '^'); view_put_char(v, '?'); return; }
    if (b < 0x20) {
        view_put_char(v, '^');
        view_put_char(v, (char)(b + 0x40));
        return;
    }
    // 0x80+：用 '?' 占位
    view_put_char(v, '?');
}

static void view_feed(view_t *v, const unsigned char *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char b = p[i];
        if (b == '\n') {
            view_newline(v);
        } else if (b == '\r') {
            v->cur_c = 0;
        } else if (b == '\b') {
            if (v->cur_c > 0) v->cur_c--;
            v->cells[v->cur_r * v->inner_cols + v->cur_c] = ' ';
        } else if (b == '\t') {
            int next = (v->cur_c + 8) & ~7;
            while (v->cur_c < next) view_put_char(v, ' ');
        } else if (b >= 0x20 && b <= 0x7e) {
            view_put_char(v, (char)b);
        } else {
            // 不支持转义控制：转成可见形式，避免外层终端执行 ESC
            view_put_caret(v, b);
        }
    }
}

static void view_render(const view_t *v) {
    const int W = v->inner_cols + 2;
    char *line = (char *)malloc((size_t)W + 1);
    if (!line) die("malloc");

    write_all(g_out_fd, "\x1b[?25l", 6); // hide cursor

    // top border
    memset(line, '-', (size_t)W);
    line[0] = '+';
    line[W - 1] = '+';
    ansi_move(v->top, v->left);
    write_all(g_out_fd, line, (size_t)W);

    // middle
    for (int r = 0; r < v->inner_rows; r++) {
        line[0] = '|';
        memcpy(line + 1, v->cells + r * v->inner_cols, (size_t)v->inner_cols);
        line[W - 1] = '|';
        ansi_move(v->top + 1 + r, v->left);
        write_all(g_out_fd, line, (size_t)W);
    }

    // bottom border
    memset(line, '-', (size_t)W);
    line[0] = '+';
    line[W - 1] = '+';
    ansi_move(v->top + v->inner_rows + 1, v->left);
    write_all(g_out_fd, line, (size_t)W);

    // place cursor inside
    ansi_move(v->top + 1 + v->cur_r, v->left + 1 + v->cur_c);
    write_all(g_out_fd, "\x1b[?25h", 6); // show cursor

    free(line);
}

static int max_int(int a, int b) { return a > b ? a : b; }

// 尝试把输入缓冲的开头解析为“一个按键动作”的字节序列长度。
// 只用于“延迟计数”，并不改变转发内容。
static size_t one_key_len(const unsigned char *p, size_t n) {
    if (n == 0) return 0;
    if (p[0] != 0x1b) return 1;

    if (n < 2) return 0;
    if (p[1] == '[' || p[1] == 'O') {
        // CSI/SS3：以 0x40..0x7E 作为终止（常见方向键等）
        for (size_t i = 2; i < n && i < 32; i++) {
            if (p[i] >= 0x40 && p[i] <= 0x7e) return i + 1;
        }
        return 0; // 不足以完成一个序列，继续等
    }
    // 其他：把 ESC 本身算一个“键”
    return 1;
}

int main(void) {
    if (!isatty(g_in_fd) || !isatty(g_out_fd)) {
        fprintf(stderr, "需要在交互式终端(tty)中运行。\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) != 0) die("sigaction(SIGCHLD)");

    // 获取当前终端尺寸
    struct winsize wsz;
    if (ioctl(g_out_fd, TIOCGWINSZ, &wsz) != 0) die("ioctl(TIOCGWINSZ)");
    int rows = (int)wsz.ws_row;
    int cols = (int)wsz.ws_col;

    int inner_rows = rows / 2;
    int inner_cols = cols / 2;
    if (inner_rows < 4 || inner_cols < 10) {
        fprintf(stderr, "终端太小（需要更大窗口）。\n");
        return 1;
    }

    view_t v;
    memset(&v, 0, sizeof(v));
    v.inner_rows = inner_rows;
    v.inner_cols = inner_cols;
    v.top  = max_int(1, (rows - (inner_rows + 2)) / 2 + 1);
    v.left = max_int(1, (cols - (inner_cols + 2)) / 2 + 1);
    v.cur_r = 0;
    v.cur_c = 0;
    v.cells = (char *)malloc((size_t)inner_rows * (size_t)inner_cols);
    if (!v.cells) die("malloc(cells)");
    memset(v.cells, ' ', (size_t)inner_rows * (size_t)inner_cols);

    // 分配 pty
    char slave_name[128];
    int mfd = pty_master_open(slave_name, sizeof(slave_name));

    // master 设为非阻塞，便于“延迟显示但持续读取”
    int fl = fcntl(mfd, F_GETFL, 0);
    if (fl < 0) die("fcntl(F_GETFL)");
    if (fcntl(mfd, F_SETFL, fl | O_NONBLOCK) != 0) die("fcntl(F_SETFL)");

    pid_t pid = fork();
    if (pid < 0) die("fork");

    if (pid == 0) {
        // child: bash on slave
        if (setsid() < 0) _exit(1);

        int sfd = open(slave_name, O_RDWR);
        if (sfd < 0) _exit(1);

        // 设置控制终端
        if (ioctl(sfd, TIOCSCTTY, 0) != 0) {
            // 有些情况下可能失败，但尽量继续
        }

        // 设置子终端窗口大小（bash 认为自己在半屏终端里）
        struct winsize cwsz;
        memset(&cwsz, 0, sizeof(cwsz));
        cwsz.ws_row = (unsigned short)inner_rows;
        cwsz.ws_col = (unsigned short)inner_cols;
        (void)ioctl(sfd, TIOCSWINSZ, &cwsz);

        dup2(sfd, STDIN_FILENO);
        dup2(sfd, STDOUT_FILENO);
        dup2(sfd, STDERR_FILENO);

        close(mfd);
        close(sfd);

        // 尽量减少转义序列输出
        setenv("TERM", "dumb", 1);
        setenv("PS1", "\\u@\\h:\\w\\$ ", 1);

        execlp("bash", "bash", "--noprofile", "--norc", (char *)NULL);
        _exit(127);
    }

    // parent
    term_raw();

    // 清屏并画初始窗口
    write_all(g_out_fd, "\x1b[2J\x1b[H", 7);
    view_render(&v);

    buf_t outq = {0};   // 子进程输出队列（延迟期间累计）
    buf_t inq  = {0};   // 用户输入队列（用于识别“一个按键动作”）
    uint64_t delay_until = 0;

    for (;;) {
        // 检查子进程
        if (g_got_sigchld) {
            g_got_sigchld = 0;
            int st;
            pid_t r = waitpid(pid, &st, WNOHANG);
            if (r == pid) break;
        }

        uint64_t now = now_ms();
        int timeout = -1;
        if (delay_until > now) {
            uint64_t rem = delay_until - now;
            timeout = (rem > 1000ull * 60ull) ? (int)(1000ull * 60ull) : (int)rem;
        } else {
            timeout = outq.len ? 0 : 50;
        }

        struct pollfd pfds[2];
        pfds[0].fd = g_in_fd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        pfds[1].fd = mfd;
        pfds[1].events = POLLIN | POLLHUP;
        pfds[1].revents = 0;

        int pr = poll(pfds, 2, timeout);
        if (pr < 0) {
            if (errno == EINTR) continue;
            die("poll");
        }

        // 读用户输入
        if (pfds[0].revents & POLLIN) {
            unsigned char tmp[256];
            ssize_t n = read(g_in_fd, tmp, sizeof(tmp));
            if (n > 0) {
                buf_append(&inq, tmp, (size_t)n);
            }
        }

        // 尝试把输入按“一个按键动作”切分转发
        for (;;) {
            if (inq.len == 0) break;
            unsigned char *p = (unsigned char *)inq.data;

            // 退出键：Ctrl-]
            if (p[0] == 0x1d) {
                goto out;
            }

            size_t klen = one_key_len(p, inq.len);
            if (klen == 0) break; // 等更多字节组成一个序列

            // 转发 klen 字节
            size_t off = 0;
            while (off < klen) {
                ssize_t w = write(mfd, p + off, klen - off);
                if (w < 0) {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN) break;
                    goto out;
                }
                off += (size_t)w;
            }

            buf_consume(&inq, klen);

            // 每次“按键动作”增加 100ms 显示延迟
            uint64_t base = delay_until > now ? delay_until : now;
            delay_until = base + 100;
            now = now_ms();
        }

        // 读子进程输出（始终读入 outq，避免子进程被写满阻塞）
        if ((pfds[1].revents & POLLIN) || (pfds[1].revents & POLLHUP)) {
            for (;;) {
                unsigned char tmp[4096];
                ssize_t n = read(mfd, tmp, sizeof(tmp));
                if (n > 0) {
                    buf_append(&outq, tmp, (size_t)n);
                    continue;
                }
                if (n == 0) goto out; // EOF
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                goto out;
            }
        }

        // 到点就刷新显示（把 outq 喂给视图并重绘）
        now = now_ms();
        if (delay_until <= now && outq.len) {
            view_feed(&v, (unsigned char *)outq.data, outq.len);
            outq.len = 0;
            view_render(&v);
        }
    }

out:
    // 结束：收尸
    kill(pid, SIGHUP);
    waitpid(pid, NULL, 0);

    free(v.cells);
    free(outq.data);
    free(inq.data);
    close(mfd);
    return 0;
}
