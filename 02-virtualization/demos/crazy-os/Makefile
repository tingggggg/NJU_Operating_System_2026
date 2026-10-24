CC       = gcc
CFLAGS   = -O2

RV_CC    = riscv64-unknown-elf-gcc
RV_FLAGS = -march=rv32ima -ffreestanding -nostdlib -march=rv32ima -mabi=ilp32 \
           -Wl,-Ttext=0x80000000

all: crazy-os p1.bin p2.bin

crazy-os: crazy-os.c mini-rv32ima.h
	$(CC) $(CFLAGS) -o $@ $<

%.elf: %.c lib.h
	$(RV_CC) $(RV_FLAGS) -o $@ $<

%.bin: %.elf
	riscv64-unknown-elf-objcopy -O binary $< $@

run: crazy-os p1.bin p2.bin
	./crazy-os p1.bin p2.bin

clean:
	rm -f crazy-os *.elf *.bin
