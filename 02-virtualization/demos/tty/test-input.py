#!/usr/bin/env python3
import sys
import tty
import termios

def main():
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        print("Press 'q' to exit.\r\n", end="", flush=True)
        while True:
            b = sys.stdin.buffer.read(1)  # 读1个字节
            print(f"{b!r}  hex={b.hex()}\r\n", end="", flush=True)
            if b == b'q':
                exit()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)

if __name__ == "__main__":
    main()
