CFLAGS = -g -O0 -Wall

all: minidbg target

minidbg: minidbg.c
	gcc $(CFLAGS) -o $@ $<

target: target.c
	gcc $(CFLAGS) -fPIE -pie -o $@ $<

run: all
	./minidbg ./target $$(nm target | awk '$$3 == "square" { print $$1 }')

clean:
	rm -f minidbg target

.PHONY: all run clean
