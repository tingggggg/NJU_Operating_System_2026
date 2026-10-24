#!/usr/bin/env python3
"""
Process address space explorer.
Parses /proc/PID/maps and disassembles executable regions.
"""

import sys
import re
from pathlib import Path
from dataclasses import dataclass
from typing import Optional

# Optional capstone for disassembly
try:
    from capstone import *
    HAS_CAPSTONE = True
except ImportError:
    HAS_CAPSTONE = False


@dataclass
class MapRegion:
    start: int
    end: int
    perms: str
    offset: int
    dev: str
    inode: int
    pathname: Optional[str]

    @property
    def size(self) -> int:
        return self.end - self.start

    @property
    def is_executable(self) -> bool:
        return 'x' in self.perms

    @property
    def is_readable(self) -> bool:
        return 'r' in self.perms


def parse_maps(pid: int) -> list[MapRegion]:
    """Parse /proc/PID/maps into list of MapRegion."""
    maps_path = Path(f"/proc/{pid}/maps")
    if not maps_path.exists():
        raise FileNotFoundError(f"No such process: {pid}")

    regions = []
    pattern = re.compile(
        r'^([0-9a-f]+)-([0-9a-f]+)\s+([rwxp-]{4})\s+([0-9a-f]+)\s+(\S+)\s+(\d+)\s*(.*)$'
    )

    for line in maps_path.read_text().strip().split('\n'):
        match = pattern.match(line)
        if not match:
            continue

        start, end, perms, offset, dev, inode, pathname = match.groups()
        pathname = pathname.strip() if pathname.strip() else None

        regions.append(MapRegion(
            start=int(start, 16),
            end=int(end, 16),
            perms=perms,
            offset=int(offset, 16),
            dev=dev,
            inode=int(inode),
            pathname=pathname
        ))

    return regions


def describe_region(region: MapRegion) -> str:
    """Return human-readable description of memory region."""
    if region.pathname:
        if region.pathname.startswith('[stack'):
            return "Thread stack"
        if region.pathname == '[heap]':
            return "Heap"
        if region.pathname.startswith('[anon:'):
            return f"Anonymous ({region.pathname[6:-1]})"
        if region.pathname.startswith('[') and region.pathname.endswith(']'):
            return region.pathname[1:-1].capitalize()
        if '.so' in region.pathname:
            return f"Shared library: {region.pathname}"
        return f"File mapping: {region.pathname}"
    return "Anonymous memory"


def get_arch(pid: int) -> tuple[int, int]:
    """Detect architecture from process exe. Returns (arch, mode) for capstone."""
    exe_path = Path(f"/proc/{pid}/exe")

    # Default to x86-64
    arch, mode = CS_ARCH_X86, CS_MODE_64

    if not exe_path.exists():
        return arch, mode

    try:
        # Read ELF header magic
        header = exe_path.read_bytes()[:20]
        if len(header) < 20 or header[:4] != b'\x7fELF':
            return arch, mode

        ei_class = header[4]  # 1=32-bit, 2=64-bit
        ei_data = header[5]   # 1=little, 2=big
        e_machine = int.from_bytes(header[18:20], 'little' if ei_data == 1 else 'big')

        if e_machine == 0x3e:  # x86-64
            arch, mode = CS_ARCH_X86, CS_MODE_64
            if ei_class == 1:
                mode = CS_MODE_32
        elif e_machine == 0x28:  # ARM
            arch, mode = CS_ARCH_ARM, CS_MODE_LITTLE_ENDIAN
        elif e_machine == 0xb7:  # AArch64
            arch, mode = CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
        elif e_machine == 0xf3:  # RISC-V
            arch = CS_ARCH_RISCV
            mode = CS_MODE_RISCV64 if ei_class == 2 else CS_MODE_RISCV32
    except Exception:
        pass

    return arch, mode


def disassemble_region(pid: int, region: MapRegion, max_bytes: int = 1024) -> None:
    """Disassemble first max_bytes of an executable region."""
    if not HAS_CAPSTONE:
        print("    [Install capstone for disassembly: pip install capstone]")
        return

    mem_path = Path(f"/proc/{pid}/mem")
    if not mem_path.exists():
        print("    [Cannot access /proc/PID/mem]")
        return

    try:
        with open(mem_path, 'rb') as f:
            f.seek(region.start)
            code = f.read(min(max_bytes, region.size))
    except (PermissionError, OSError) as e:
        print(f"    [Cannot read memory: {e}]")
        return

    if not code:
        print("    [Empty region]")
        return

    arch, mode = get_arch(pid)

    try:
        md = Cs(arch, mode)
        md.detail = False
        md.skipdata = True  # Skip invalid instructions instead of stopping

        print(f"    First {len(code)} bytes disassembled:")
        for insn in md.disasm(code, region.start):
            bytes_str = ' '.join(f'{b:02x}' for b in insn.bytes[:8])
            if len(insn.bytes) > 8:
                bytes_str += '...'
            print(f"      0x{insn.address:016x}: {bytes_str:<20} {insn.mnemonic} {insn.op_str}")
    except Exception as e:
        print(f"    [Disassembly error: {e}]")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <pid>")
        sys.exit(1)

    try:
        pid = int(sys.argv[1])
    except ValueError:
        print(f"Invalid PID: {sys.argv[1]}")
        sys.exit(1)

    try:
        regions = parse_maps(pid)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except PermissionError:
        print(f"Permission denied: cannot read /proc/{pid}/maps")
        print("Try running with sudo for system processes")
        sys.exit(1)

    print(f"Process {pid} Address Space ({len(regions)} regions)")
    print("=" * 80)

    for i, r in enumerate(regions, 1):
        print(f"\n[{i}] 0x{r.start:016x}-0x{r.end:016x} ({r.size:,} bytes)")
        print(f"    Permissions: {r.perms}")
        print(f"    Description: {describe_region(r)}")

        if r.pathname and r.offset > 0:
            print(f"    File offset: 0x{r.offset:x}")

        if r.is_executable and r.is_readable:
            disassemble_region(pid, r)


if __name__ == "__main__":
    main()
