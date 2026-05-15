#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

pid_t find_pid(const char* name) {
    DIR* d = opendir("/proc");
    if (!d) {
        perror("Failed to open /proc");
        return -1;
    }
    
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_type != DT_DIR) continue;
        
        pid_t pid = (pid_t)atoi(e->d_name);
        if (pid <= 0) continue;
        
        char path[PATH_MAX], exe[PATH_MAX];
        snprintf(path, sizeof(path), "/proc/%d/exe", pid);
        
        ssize_t len = readlink(path, exe, sizeof(exe) - 1);
        if (len != -1) {
            exe[len] = '\0';
            if (strstr(exe, name)) {
                closedir(d);
                fprintf(stderr, "Found PID %d for process %s\n", pid, name);
                return pid;
            }
        }
    }
    closedir(d);
    fprintf(stderr, "Process '%s' not found\n", name);
    return -1;
}

unsigned long get_module_base(pid_t pid, const char *name) {
    char path[64], line[512];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
        return 0;
    }
    
    unsigned long addr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            addr = strtoul(line, NULL, 16);
            fprintf(stderr, "Module %s base: 0x%lx\n", name, addr);
            break;
        }
    }
    fclose(f);
    return addr;
}

unsigned long get_offset(const char *lib, const char *sym) {
    void *h = dlopen(lib, RTLD_LAZY);
    if (!h) {
        fprintf(stderr, "Cannot open %s: %s\n", lib, dlerror());
        return 0;
    }
    
    void *sym_addr = dlsym(h, sym);
    if (!sym_addr) {
        fprintf(stderr, "Cannot find symbol %s: %s\n", sym, dlerror());
        dlclose(h);
        return 0;
    }
    
    unsigned long offset = (unsigned long)sym_addr - get_module_base(getpid(), lib);
    dlclose(h);
    fprintf(stderr, "Symbol %s offset: 0x%lx\n", sym, offset);
    return offset;
}

void inject(pid_t pid, const char *lib_path) {
    struct user_regs_struct old_regs, regs;
    unsigned long target_dlopen;
    int status;
    
    fprintf(stderr, "Attempting to attach to PID %d\n", pid);
    
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
        fprintf(stderr, "PTRACE_ATTACH failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed: %s\n", strerror(errno));
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "Process attached, reading registers...\n");
    
    if (ptrace(PTRACE_GETREGS, pid, NULL, &old_regs) < 0) {
        fprintf(stderr, "PTRACE_GETREGS failed: %s\n", strerror(errno));
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    memcpy(&regs, &old_regs, sizeof(regs));
    
    unsigned long libc = get_module_base(pid, "libc.so.6");
    unsigned long libdl = get_module_base(pid, "libdl.so.2");
    
    if (libdl) {
        target_dlopen = libdl + get_offset("libdl.so.2", "dlopen");
    } else if (libc) {
        target_dlopen = libc + get_offset("libc.so.6", "dlopen");
    } else {
        fprintf(stderr, "Cannot find libc or libdl\n");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    if (!target_dlopen) {
        fprintf(stderr, "Cannot find dlopen function\n");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "dlopen address: 0x%lx\n", target_dlopen);
    fprintf(stderr, "Injecting library: %s\n", lib_path);
    
    regs.rsp -= 0x200;
    size_t len = strlen(lib_path) + 1;
    
    for (size_t i = 0; i < len; i += sizeof(long)) {
        long word = 0;
        memcpy(&word, lib_path + i, (len - i < sizeof(long)) ? len - i : sizeof(long));
        if (ptrace(PTRACE_POKEDATA, pid, regs.rsp + i, word) < 0) {
            fprintf(stderr, "PTRACE_POKEDATA failed at offset %zu: %s\n", i, strerror(errno));
            ptrace(PTRACE_DETACH, pid, NULL, NULL);
            exit(EXIT_FAILURE);
        }
    }
    
    regs.rdi = regs.rsp;        // First arg: lib path
    regs.rsi = 2;              // RTLD_NOW = 2
    regs.rip = target_dlopen;   // Call dlopen
    regs.rsp -= 16;            // Align stack and make space for return address
    
    if (ptrace(PTRACE_POKEDATA, pid, regs.rsp, 0) < 0) {
        fprintf(stderr, "Failed to write return address: %s\n", strerror(errno));
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    if (ptrace(PTRACE_SETREGS, pid, NULL, &regs) < 0) {
        fprintf(stderr, "PTRACE_SETREGS failed: %s\n", strerror(errno));
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "Continuing process to execute dlopen...\n");
    
    if (ptrace(PTRACE_CONT, pid, NULL, NULL) < 0) {
        fprintf(stderr, "PTRACE_CONT failed: %s\n", strerror(errno));
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    sleep(1);
    
    if (waitpid(pid, &status, 0) == -1) {
        fprintf(stderr, "waitpid failed after cont: %s\n", strerror(errno));
    }
    
    if (ptrace(PTRACE_SETREGS, pid, NULL, &old_regs) < 0) {
        fprintf(stderr, "Failed to restore registers: %s\n", strerror(errno));
    }
    
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) < 0) {
        fprintf(stderr, "PTRACE_DETACH failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "Injection complete!\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <pid> [lib_path]\n", argv[0]);
        fprintf(stderr, "  or : %s sober [lib_path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    pid_t pid;
    const char *lib_path = (argc > 2) ? argv[2] : "./sober_test_inject.so";
    
    if (isdigit(argv[1][0])) {
        pid = (pid_t)atoi(argv[1]);
    } else {
        pid = find_pid(argv[1]);
    }
    
    if (pid == -1) {
        fprintf(stderr, "Error: Cannot find process\n");
        return EXIT_FAILURE;
    }
    
    fprintf(stderr, "Starting injection for PID %d with library %s\n", pid, lib_path);
    inject(pid, lib_path);
    
    return EXIT_SUCCESS;
}
