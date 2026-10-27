// minidbg.c — a ~50-line software-breakpoint debugger (Linux x86-64, PIE target)
// usage: ./minidbg ./target <offset-hex>      offset from `nm target | grep square`
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

// PIE: the first line of /proc/pid/maps is where the ELF was mmap'ed (load base)
static unsigned long load_base(pid_t pid) {
  char path[64]; unsigned long base;
  snprintf(path, sizeof path, "/proc/%d/maps", pid);
  FILE *f = fopen(path, "r");
  fscanf(f, "%lx", &base);
  fclose(f);
  return base;
}

int main(int argc, char *argv[]) {
  pid_t pid = fork();
  if (pid == 0) {                                // child: become the tracee
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    execv(argv[1], (char *[]){ argv[1], NULL }); // exec stops us with SIGTRAP
    perror("execv"); exit(1);
  }
  int status;
  waitpid(pid, &status, 0);                      // [A] tracee stopped right after exec

  unsigned long addr = load_base(pid) + strtoul(argv[2], NULL, 16);
  long orig = ptrace(PTRACE_PEEKTEXT, pid, addr, 0);
  long trap = (orig & ~0xffL) | 0xcc;            // [B] patch the low byte to int3
  ptrace(PTRACE_POKETEXT, pid, addr, trap);
  printf("[dbg] breakpoint @ %#lx: %02lx -> cc\n", addr, orig & 0xff);

  for (int hits = 1; ; hits++) {
    ptrace(PTRACE_CONT, pid, 0, 0);              // [C] run until the next trap
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) break;

    struct user_regs_struct regs;                // [D] trap hit: rip = addr + 1
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    printf("[dbg] hit #%d  rip=%#llx  arg(rdi)=%lld\n", hits, regs.rip, regs.rdi);

    ptrace(PTRACE_POKETEXT, pid, addr, orig);    // [E1] restore original byte
    regs.rip = addr;                             // [E2] rewind rip by 1
    ptrace(PTRACE_SETREGS, pid, 0, &regs);
    ptrace(PTRACE_SINGLESTEP, pid, 0, 0);        // [E3] execute the real instruction
    waitpid(pid, &status, 0);
    ptrace(PTRACE_POKETEXT, pid, addr, trap);    // [E4] re-arm the breakpoint
  }
  printf("[dbg] tracee exited with %d\n", WEXITSTATUS(status));
  return 0;
}
