#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mini-rv32ima.h"

#define MEM_SIZE   (1 << 20)
#define MEM_OFFSET 0x80000000u
#define STACK_TOP  (MEM_OFFSET + MEM_SIZE)


struct proc {
    // Process "virtual machine" state:
    // Register & memory
    struct CPUState cpu;
    uint8_t mem[MEM_SIZE];

    // Operating-system internal state
    char buf[256];
    int buf_len;
};

static void proc_init(struct proc *p, const char *path) {
    FILE *f = fopen(path, "rb");
    fread(p->mem, 1, MEM_SIZE, f);
    fclose(f);

    p->cpu.mem = p->mem;
    p->cpu.mem_offset = MEM_OFFSET;
    p->cpu.mem_size = MEM_SIZE;
    memset(p->cpu.regs, 0, sizeof(p->cpu.regs));
    memset(p->cpu.csrs, 0, sizeof(p->cpu.csrs));
    p->cpu.csrs[PC] = MEM_OFFSET;
    p->cpu.regs[SP] = STACK_TOP;
}

static int sys_putchar(struct proc *p, char ch) {
    p->buf[p->buf_len++] = ch;
    if (ch == '\n' || p->buf_len == sizeof(p->buf) - 1) {
        fwrite(p->buf, 1, p->buf_len, stdout);
        fflush(stdout);
        p->buf_len = 0;
    }
    return 0;
}

static void handle_ecall(struct proc *p) {
    int ret = -1;

    switch (p->cpu.regs[A7]) {
        case 42:
            ret = sys_putchar(p, p->cpu.regs[A0]); break;
    }

    p->cpu.regs[A0] = ret;

    // Replicate what MRET does: restore privilege/interrupt state and
    // resume user execution at the instruction after the ecall.
    uint32_t ms = p->cpu.csrs[MSTATUS];
    uint32_t ef = p->cpu.csrs[EXTRAFLAGS];
    p->cpu.csrs[MSTATUS]    = ((ms & 0x80) >> 4) | ((ef & 3) << 11) | 0x80;
    p->cpu.csrs[EXTRAFLAGS] = (ef & ~3) | ((ms >> 11) & 3);
    p->cpu.csrs[PC]         = p->cpu.csrs[MEPC] + 4;
    p->cpu.csrs[MCAUSE]     = 0;
}

int main(int argc, char *argv[]) {
    int n = argc - 1;
    struct proc *procs = calloc(n, sizeof(struct proc));
    for (int i = 0; i < n; i++)
        proc_init(&procs[i], argv[i + 1]);

    int cur = 0;
    while (1) {
        struct proc *p = &procs[cur];
        rv32ima_step(&p->cpu, 1);
        if (p->cpu.csrs[MCAUSE] == 8)
            handle_ecall(p);
        cur = (cur + 1) % n;
    }
}
