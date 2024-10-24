# Building MatIEC

## Prerequisites

### Linux

Install the required build tools:

```bash
# Debian/Ubuntu
sudo apt-get install build-essential autoconf automake libtool

# Fedora/RHEL
sudo dnf install gcc gcc-c++ autoconf automake libtool
```

### Windows (MSYS2)

1. Install MSYS2 from https://www.msys2.org/
2. Open MSYS2 MinGW x64 (`mingw64.exe`)
3. Install required packages:

```bash
pacman -S autoconf automake libtool make gcc
```

## Build Instructions

### Linux Native Build

```bash
autoreconf -i
./configure
make
```

### Windows Native Build (MSYS2 MinGW64)

```bash
aclocal
autoheader
automake --add-missing
autoconf
./configure
make
```

### Cross-Compiling on Linux for Windows

```bash
# Basic build
./configure --host=i586-pc-mingw32
make

# Or with static linking (no MinGW DLL dependencies)
./configure --host=i586-pc-mingw32 LDFLAGS="-static"
make
```

## Maintenance Tasks

### Adding New Files

1. Add new files to `Makefile.am` or create a new makefile
2. Run:

```bash
autoreconf
```

### Cleaning Project

```bash
make distclean
```

### Version Control Ignore Patterns

Add these patterns to your `.gitignore` or `.hgignore`:

```
Makefile
config.*
*.a
.deps
```

## Troubleshooting

### Common Issues

- If `configure` script is missing, run the autotools chain:
  ```bash
  aclocal
  autoheader
  automake --add-missing
  autoconf
  ```
- If you see warnings about AC_PROG_LEX, they can be safely ignored during build

## Support

For build system improvements or suggestions, contact:
matteo.facchinetti@sirius-es.it
