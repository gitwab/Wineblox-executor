CC = gcc
CFLAGS = -Wall -Wextra -fPIC -D_GNU_SOURCE -O2
GTK_CFLAGS = $(shell pkg-config --cflags gtk4 2>/dev/null || echo "-I/usr/include/gtk-4.0")
GTK_LIBS = $(shell pkg-config --libs gtk4 2>/dev/null || echo "-lgtk-4")
CURL_CFLAGS = $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS = $(shell pkg-config --libs libcurl 2>/dev/null || echo "-lcurl")
JSON_CFLAGS = $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS = $(shell pkg-config --libs json-c 2>/dev/null || echo "-ljson-c")
LUA_CFLAGS = $(shell pkg-config --cflags lua5.1 2>/dev/null || pkg-config --cflags lua 2>/dev/null || echo "")
LUA_LIBS = $(shell pkg-config --libs lua5.1 2>/dev/null || pkg-config --libs lua 2>/dev/null || echo "")

.PHONY: all clean install check-deps help

all: check-deps atingle_enhanced injector sober_test_inject.so

check-deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || { echo "Error: gcc not found. Install build-essential."; exit 1; }
	@pkg-config --exists gtk4 || { echo "Warning: GTK4 not found. Install libgtk-4-dev or gtk4-devel."; }
	@pkg-config --exists libcurl || { echo "Warning: libcurl not found. Install libcurl4-openssl-dev."; }
	@pkg-config --exists json-c || { echo "Warning: json-c not found. Install libjson-c-dev."; }

# Enhanced UI with Script Hub
atingle_enhanced: AtingleUI_Enhanced.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(CURL_CFLAGS) $(JSON_CFLAGS) AtingleUI_Enhanced.c -o atingle $(GTK_LIBS) $(CURL_LIBS) $(JSON_LIBS) -lpthread
	@echo "✓ Built: atingle (Enhanced)"

# Legacy UI executable (optional)
atingle: AtingleUI.c
	$(CC) $(CFLAGS) $(GTK_CFLAGS) AtingleUI.c -o atingle $(GTK_LIBS) -lpthread
	@echo "✓ Built: atingle"

# Injector executable
injector: Injector.c
	$(CC) $(CFLAGS) Injector.c -o injector -ldl
	@echo "✓ Built: injector"

# Shared library to inject
sober_test_inject.so: injected_lib.c
	$(CC) $(CFLAGS) -shared injected_lib.c $(LUA_CFLAGS) -o sober_test_inject.so $(LUA_LIBS) -ldl
	@echo "✓ Built: sober_test_inject.so"

clean:
	rm -f atingle atingle_enhanced injector sober_test_inject.so *.o
	@echo "Cleaned"

help:
	@echo "Wineblox Executor - Build targets:"
	@echo "  make              - Build all components (enhanced UI + injector)"
	@echo "  make atingle_enhanced - Build enhanced UI with Script Hub"
	@echo "  make atingle      - Build legacy UI"
	@echo "  make injector     - Build injector only"
	@echo "  make clean        - Remove all built files"
	@echo "  make check-deps   - Check for required dependencies"
	@echo "  make help         - Show this help message"
