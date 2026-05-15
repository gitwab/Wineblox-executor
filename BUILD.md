# Diesel Executor - Build Guide

## System Requirements
- **OS**: Linux (Ubuntu, Debian, Fedora, Arch, Linux Mint, etc.)
- **Kernel**: 4.4+
- **Architecture**: x86_64

## Installation

### Ubuntu/Debian (including Linux Mint)
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libgtk-4-dev \
  libglib2.0-dev \
  liblua5.1-0-dev \
  pkg-config
```

### Fedora/RHEL/CentOS
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
sudo pacman -Syu
sudo pacman -S base-devel gtk4 lua pkg-config
```

## Building

### Quick Build
```bash
cd Diesel-executor
make clean
make
```

### Verify Build
```bash
ls -la atingle injector sober_test_inject.so
```

You should see three executable/library files.

## Running

### Prerequisites
1. **Linux Mint/Ubuntu** - You need Sober client installed and running
2. **Permissions** - ptrace injection requires same user or elevated privileges

### Start Sober Client
```bash
# In another terminal, start the Sober client with Roblox
# This is required before using the executor
```

### Launch Executor
```bash
./atingle
```

A GTK window will open with:
- **Text Editor** - Write Lua scripts
- **Buttons**:
  - `Execute` - Execute current script
  - `Clear` - Clear editor
  - `Open File` - Load script from disk
  - `Save File` - Save script to disk
  - `Attach` - Inject into Sober process

### Status Display
Watch the status bar at the bottom for injection results and errors.

## Troubleshooting

### Build Errors

**"gtk4 not found"**
```bash
# Verify GTK4 installation
pkg-config --cflags --libs gtk4

# Reinstall if needed
sudo apt-get install --reinstall libgtk-4-dev
```

**"gcc: command not found"**
```bash
# Install build tools
sudo apt-get install build-essential  # Ubuntu/Debian
sudo dnf install gcc make             # Fedora
sudo pacman -S base-devel             # Arch
```

### Runtime Errors

**"Sober process not found"**
- Make sure Sober client is running
- Check with: `ps aux | grep sober`

**"Error: injector binary not found"**
- Run from the directory containing the compiled `injector` binary
- Make sure `make` completed successfully

**"Injection failed"**
- ptrace might require elevated privileges
- Try: `sudo ./atingle`
- Or check kernel ptrace restrictions: `cat /proc/sys/kernel/yama/ptrace_scope`

### Linux Mint Specific

Linux Mint is based on Ubuntu/Debian, so use the Ubuntu/Debian instructions above.

If you have issues with GTK4:
```bash
# Mint uses apt just like Ubuntu
sudo apt-get install -y libgtk-4-dev libgtk-4-1
```

## Advanced Usage

### Using injector standalone
```bash
# By process name
./injector sober /path/to/library.so

# By PID
./injector 1234 /path/to/library.so
```

### Debug mode
```bash
# Run with output redirection
./atingle 2>&1 | tee debug.log
```

## Cleanup
```bash
make clean
```
