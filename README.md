# Wineblox Executor

> A functional Roblox executor for Linux using Wine

[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org/)
[![Platform: Linux](https://img.shields.io/badge/platform-Linux-yellow)]()
[![Language: C](https://img.shields.io/badge/language-C-red)]()

## Stort hiere

```bash
# Clone and build
git clone https://github.com/gitwab/Wineblox-executor.git
cd Wineblox-executor
chmod +x build.sh
./build.sh

# Run
sudo ./injector sober
```

## What it can do for now
 - Inject basic scripts
 - Could be unstable
## Requirements (yes)

- Linux OS (Ubuntu/Debian, Fedora, Arch)
- Wine (for running ROBLOX)
- GCC compiler
- Lua 5.1 development libraries
- GTK4 (optional, for GUI)
- libcurl (optional, for Script Hub)

##  Installation

### Easy Way (Recommended)
```bash
git clone https://github.com/gitwab/Wineblox-executor.git
cd Wineblox-executor
chmod +x build.sh
./build.sh
```

### Manual Build
```bash
make check-deps      # Verify dependencies
make                 # Build all components
```

See [INSTALL.md](INSTALL.md) for detailed instructions.

## Usage

### Basic Injection
```bash
# Start ROBLOX on Wine
WINEPREFIX=~/.wine wine ~/.wine/drive_c/Program\ Files/Roblox/Versions/*/RobloxPlayerBeta.exe

# In another terminal, inject the executor
sudo ./injector sober
```

### Advanced Usage
```bash
# Specify PID manually
sudo ./injector <PID> ./sober_test_inject.so

# Custom library path
sudo ./injector sober /path/to/library.so

# Launch UI
./atingle_enhanced
```
## Troubleshooting

| Issue | Solution |
|-------|----------|
| `PTRACE_ATTACH failed: Operation not permitted` | Run with `sudo` or check ptrace_scope |
| `Process 'sober' not found` | Ensure ROBLOX is running before injection |
| `Cannot find libc.so.6` | System libraries may be inaccessible |
| Build fails on dependency | Check [INSTALL.md](INSTALL.md) for your distro |

See [INSTALL.md#troubleshooting](INSTALL.md#troubleshooting) for more detailed solutions.

## Build Targets

```bash
make                    # Build everything
make check-deps         # Verify dependencies
make atingle_enhanced   # Enhanced UI with Script Hub
make injector           # Injector only
make sober_test_inject.so  # Library only
make clean              # Remove built files
make help               # Show all targets
```

## To know

- Requires elevated privileges (`sudo`) to attach to processes
- May be detected by anti-cheat systems
- Only use on systems you own or have explicit permission
- Intended for educational and authorized testing purposes only

## Components

| Component | Type | Purpose |
|-----------|------|---------|
| `Injector.c` | Source | ptrace-based process injector |
| `injected_lib.c` | Source | Lua runtime injected into ROBLOX |
| `AtingleUI_Enhanced.c` | Source | GTK4 graphical interface |
| `Makefile` | Build | Compilation rules and targets |
| `build.sh` | Script | Automated build wrapper |

## Contributing

Found a bug or have a feature request? 
- Submit an [Issue](https://github.com/gitwab/Wineblox-executor/issues)
- Create a [Pull Request](https://github.com/gitwab/Wineblox-executor/pulls)
- Trust me this has inf bugs

## License

This project is released under the [Unlicense](https://unlicense.org/) - ye

## Credits

- Based on [AtingleExecutor](https://github.com/AtingleTeam/AtingleExecutor)
- ptrace injection technique inspired by reverse engineering community
- Lua integration for ROBLOX scripting

---

<div align="center">

[⬆ back to top,good boy!](#wineblox-executor)

</div>
