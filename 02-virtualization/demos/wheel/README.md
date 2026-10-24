# 变速齿轮

实现 “变速齿轮”：通过 LD_PRELOAD 机制覆盖和时间相关的系统调用 (gettimeofday, usleep, alarm 等)，通过在启动时记录时间并维护 10 倍速度的虚拟时间实现变速。
