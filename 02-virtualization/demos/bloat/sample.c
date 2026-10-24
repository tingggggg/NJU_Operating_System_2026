#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>

int main() {
    // This is vibe-coded. Asked AI to load dynamic library.
    void *handle = dlopen("./libbloat.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        _exit(1);
    }
    void (*bloat)() = (void (*)())dlsym(handle, "bloat");
    if (!bloat) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        _exit(1);
    }

    bloat();
    printf("Executed bloat() at %p\n", (void*)bloat);
    sleep(1000);
}
