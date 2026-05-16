#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Executor API functions
static int executor_print(lua_State *L) {
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; i++) {
        const char *str = lua_tostring(L, i);
        if (str) printf("%s\t", str);
    }
    printf("\n");
    return 0;
}

static int executor_warn(lua_State *L) {
    fprintf(stderr, "[WARNING] ");
    int nargs = lua_gettop(L);
    for (int i = 1; i <= nargs; i++) {
        const char *str = lua_tostring(L, i);
        if (str) fprintf(stderr, "%s\t", str);
    }
    fprintf(stderr, "\n");
    return 0;
}

static int executor_assert(lua_State *L) {
    int cond = lua_toboolean(L, 1);
    if (!cond) {
        const char *msg = lua_tostring(L, 2);
        luaL_error(L, msg ? msg : "assertion failed");
    }
    return 0;
}

// Initialize executor environment
static void init_executor(lua_State *L) {
    // Create executor table
    lua_newtable(L);
    
    // Add functions
    lua_pushcfunction(L, executor_print);
    lua_setfield(L, -2, "print");
    
    lua_pushcfunction(L, executor_warn);
    lua_setfield(L, -2, "warn");
    
    lua_pushcfunction(L, executor_assert);
    lua_setfield(L, -2, "assert");
    
    lua_setglobal(L, "executor");
}

// Library initialization
int luaopen_sober_test_inject(lua_State *L) {
    printf("[INJECT] Executor library loaded!\n");
    init_executor(L);
    return 0;
}

// Execution context
__attribute__((constructor))
void executor_init(void) {
    fprintf(stderr, "[INJECT] Wineblox executor initialized\n");
}

__attribute__((destructor))
void executor_fini(void) {
    fprintf(stderr, "[INJECT] Wineblox executor unloaded\n");
}
