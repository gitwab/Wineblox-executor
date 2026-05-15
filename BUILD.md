# Building Diesel Executor

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libgtk-4-dev \
  libglib2.0-dev \
  liblua5.1-0-dev \
  pkg-config
```

### Fedora/RHEL
```bash
sudo dnf install -y \
  gcc \
  make \
  gtk4-devel \
  glib2-devel \
  lua-devel \
  pkg-config
```

### Arch Linux
```bash
sudo pacman -S base-devel gtk4 lua
```

## Build Steps

1. **Clone or navigate to repository:**
```bash
cd Diesel-executor
```

2. **Build all components:**
```bash
make clean
make
```

This will generate three files:
- `atingle` - GTK4 GUI application
- `injector` - ptrace-based injection tool
- `sober_test_inject.so` - Shared library to inject

## Running

1. **Start the Sober client first** (on Linux, with Roblox)

2. **Run the GUI:**
```bash
./atingle
```

3. **In the GUI:**
   - Write Lua scripts in the text editor
   - Click **"Attach"** to inject into Sober process
   - Click **"Execute"** to run scripts

## Files Generated

| File | Purpose |
|------|---------|
| `atingle` | Main executable with GTK4 GUI |
| `injector` | Helper tool for ptrace injection |
| `sober_test_inject.so` | Library containing Lua runtime |

## Troubleshooting

### "Process with name containing 'sober' not found"
- Sober client is not running
- Start Sober/Roblox first before clicking Attach

### "Injector failed"
- May need elevated privileges for ptrace
- Try: `sudo ./atingle`

### Build fails with GTK4 errors
```bash
# Verify GTK4 is installed:
pkg-config --cflags --libs gtk4
```

### Lua library not found
```bash
# Find lua packages:
pkg-config --list-all | grep lua
```

## Clean Build

```bash
make clean
make
```
