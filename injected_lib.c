#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

unsigned long get_lib_base(const char* name) {
    char line[512];
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    
    unsigned long addr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            addr = strtoul(line, NULL, 16);
            break;
        }
    }
    fclose(f);
    return addr;
}

void print_to_console(const char* message) {
    fprintf(stderr, "[Injected] %s\n", message);
    fflush(stderr);
}

__attribute__((constructor))
void init_lib() {
    print_to_console("Diesel executor library loaded successfully!");
    print_to_console("Library base: 0x%lx", get_lib_base("sober_test_inject.so"));
}
