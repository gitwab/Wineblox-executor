# Wineblox Executor - Build & Installation Guide

## System Requirements
- **OS**: Linux (Ubuntu, Debian, Fedora, Arch, Linux Mint, etc.)
- **Kernel**: 4.4+
- **Architecture**: x86_64
- **RAM**: 512MB+

## Installation

### Ubuntu/Debian/Linux Mint
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  libgtk-4-dev \
  libglib2.0-dev \
  liblua5.1-0-dev \
  libcurl4-openssl-dev \
  libjson-c-dev \
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
  libcurl-devel \
  json-c-devel \
  pkg-config
```

### Arch Linux
```bash
sudo pacman -Syu
sudo pacman -S base-devel gtk4 lua curl json-c pkg-config
```

## Building

### Quick Build (with Script Hub)
```bash
cd Wineblox-executor
make clean
make
```

### Build Legacy UI Only
```bash
make atingle
```

### Verify Build
```bash
ls -la atingle injector sober_test_inject.so
```

## Running

### Prerequisites
1. **Sober Client** must be installed and running (inject target)
2. **Permissions** - ptrace injection typically requires:
   - Same user privileges, OR
   - Elevated privileges (sudo)

### Launch Executor
```bash
./atingle
```

### If Permissions Denied
```bash
sudo ./atingle
```

## UI Features

### Editor Tab
- **Text Editor** - Write/edit Lua scripts
- **Execute** - Execute script in Sober process
- **Clear** - Clear editor contents
- **Open File** - Load script from disk (.lua files)
- **Save File** - Save script to disk
- **Attach** - Inject library into Sober process

### Script Hub Tab (NEW)
- **Category Selector** - Browse scripts by category:
  - Roblox (general scripts)
  - Game (game-specific exploits)
  - Exploits (utility exploits)
  - Utilities (helper tools)
- **Script List** - Live list from ScriptBlox API
- **Load Script** - Load script directly into editor

## Troubleshooting

### Build Errors

**"gtk4 not found"**
```bash
pkg-config --cflags --libs gtk4
sudo apt-get install --reinstall libgtk-4-dev  # Ubuntu/Debian
sudo dnf install --reinstall gtk4-devel        # Fedora
sudo pacman -S gtk4                             # Arch
```

**"libcurl not found"**
```bash
sudo apt-get install libcurl4-openssl-dev  # Ubuntu/Debian
sudo dnf install libcurl-devel              # Fedora
sudo pacman -S curl                         # Arch
```

**"json-c not found"**
```bash
sudo apt-get install libjson-c-dev  # Ubuntu/Debian
sudo dnf install json-c-devel       # Fedora
sudo pacman -S json-c               # Arch
```

### Runtime Errors

**"Sober process not found"**
- Ensure Sober client is running: `ps aux | grep sober`
- Start Sober with Roblox game loaded
- Check if process name matches "sober"

**"Injection failed"**
- May require elevated privileges: `sudo ./atingle`
- Check kernel ptrace restrictions:
  ```bash
  cat /proc/sys/kernel/yama/ptrace_scope
  # 0 = permissive, 1 = restricted to same user, 2 = restricted to parent, 3 = disabled
  ```
- If restricted, either use same user or sudo

**"Script Hub not loading"**
- Check internet connection
- Verify curl/json-c libraries are properly installed
- Test ScriptBlox API: `curl https://scriptblox.com/api/script/search?category=roblox`

**"GTK initialization failed"**
- Run with debug output:
  ```bash
  GTK_DEBUG=all ./atingle 2>&1 | head -50
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
./atingle 2>&1 | tee debug.log
```

### Check injection success
```bash
cat /proc/$(pgrep sober)/maps | grep injected
```

## Architecture

### Components
1. **AtingleUI_Enhanced.c** - GTK4 UI with Script Hub integration
2. **Injector.c** - ptrace-based DLL injector for Linux
3. **injected_lib.c** - Lua runtime library injected into process
4. **init.lua** - Executor API environment setup

### Process Flow
1. Launch `atingle` UI
2. Select "Script Hub" tab to browse/load scripts
3. Edit script in "Editor" tab
4. Click "Attach" to inject library into Sober
5. Click "Execute" to run Lua script

## Cleanup
```bash
make clean
```

## License
This project is under The Unlicense (public domain)
