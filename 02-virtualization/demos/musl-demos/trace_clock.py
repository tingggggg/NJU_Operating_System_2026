# Usage: gdb -x trace_clock.py
# trace_clock

import gdb

LOG_PATH = "inst.log"

def frame_depth():
    f = gdb.newest_frame()
    d = 0
    while f is not None:
        d += 1
        f = f.older()
    return d

def cur_func_name():
    f = gdb.selected_frame()
    if f is None:
        return "??"
    n = f.name()
    return n if n else "??"

class TraceKernelClock(gdb.Command):
    """
trace_clock [max_steps]
运行到 __kernel_clock_gettime 后开始单步(si)并记录指令到 inst.log，
直到从 __kernel_clock_gettime 返回。max_steps 默认 1000000。
"""

    def __init__(self):
        super(TraceKernelClock, self).__init__("trace_clock", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        arg = arg.strip()
        max_steps = int(arg) if arg else 1000000

        gdb.execute("set pagination off")
        gdb.execute("set breakpoint pending on")

        # 临时断点：第一次进入 __kernel_clock_gettime 就停下
        gdb.Breakpoint("__kernel_clock_gettime", temporary=True)

        # 运行到断点
        gdb.execute("run")

        entry_depth = frame_depth()

        with open(LOG_PATH, "w", buffering=1) as fp:
            steps = 0
            while True:
                # 记录“将要执行”的当前指令
                try:
                    pc = int(gdb.parse_and_eval("$pc"))
                    dis = gdb.execute("x/i $pc", to_string=True).strip()
                    fn = cur_func_name()
                    fp.write(f"{steps:08d} {fn} {dis}\n")
                except gdb.error as e:
                    fp.write(f"== 记录失败: {e} ==\n")
                    break

                steps += 1
                if steps >= max_steps:
                    fp.write("== 达到 max_steps，停止 ==\n")
                    break

                # 单步执行一条指令（会步入 call 目标）
                try:
                    gdb.execute("si", to_string=True)
                except gdb.error as e:
                    fp.write(f"== si 失败/程序可能已退出: {e} ==\n")
                    break

                # 从 __kernel_clock_gettime 返回后，栈深度会小于入口时的深度
                if gdb.selected_inferior().pid == 0:
                    fp.write("== inferior 已退出 ==\n")
                    break
                if frame_depth() < entry_depth:
                    fp.write("== 已从 __kernel_clock_gettime 返回，停止 ==\n")
                    break

TraceKernelClock()
