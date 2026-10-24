INIT := /init
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
CROSS :=
else
CROSS := x86_64-linux-gnu-
endif

initramfs: minimal
# Copy kernel and minimal from the host system
	@mkdir -p build/initramfs/bin
	sudo bash -c "cp /boot/vmlinuz build/ && chmod 666 build/vmlinuz"
	cp minimal build/initramfs/init

# Pack build/initramfs as gzipped cpio archive
	cd build/initramfs && \
	  find . -print0 \
	  | cpio --null -ov --format=newc \
	  | gzip -9 > ../initramfs.cpio.gz

run:
# Run QEMU with the installed kernel and generated initramfs
	qemu-system-x86_64 \
	  -nographic \
	  -serial mon:stdio \
	  -kernel build/vmlinuz \
	  -initrd build/initramfs.cpio.gz \
	  -machine accel=kvm:tcg \
	  -append "console=ttyS0 rdinit=$(INIT)"

clean:
	rm -rf build minimal.s minimal.o minimal

.PHONY: initramfs run clean minimal

minimal: minimal.S
	$(CROSS)gcc -g -S minimal.S > minimal.s
	$(CROSS)as minimal.s -o minimal.o
	$(CROSS)ld -o minimal minimal.o
