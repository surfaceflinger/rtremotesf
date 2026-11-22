# Rtremotesf
Remote GUI for rtorrent. Supports GNU/Linux, Windows and macOS.

Table of Contents
=================

   * [Rtremotesf](#rtremotesf)
   * [Table of Contents](#table-of-contents)
      * [Connecting to rtorrent](#connecting-to-rtorrent)
      * [Installation](#installation)
         * [Dependencies](#dependencies)
         * [Building](#building)
         * [Nix](#nix)
         * [Windows](#windows)
         * [macOS](#macos)

## Connecting to rtorrent
Rtremotesf talks to rtorrent's XML-RPC interface directly over SCGI. Enable an SCGI listener in `rtorrent.rc`:

```ini
# TCP
network.scgi.open_port = 127.0.0.1:5000

# or a Unix socket
# network.scgi.open_local = /run/user/1000/rtorrent.sock
```

rtorrent's XML-RPC interface has no authentication or encryption. If it is not bound to a local address,
protect it with an SSH tunnel, a VPN, or a reverse proxy. In Rtremotesf's server settings use the same address and
port, or the Unix socket path.

## Installation
This fork is distributed as source. Use the Nix flake or build it from source.

### Dependencies
- C++ compiler with partial C++23 support. Minimum tested versions of GCC and Clang toolchains:
  1. GCC 14
  2. Clang 19 with libstdc++ 14
  3. Clang 19 with libc++ 19
- CMake 3.31 or newer
- Qt 6.8 or newer (Core, Network, Gui, Widgets modules)
- fmt 10.1.0 or newer
- KWidgetsAddons 6.11 or newer
- libpsl 0.21.2 or newer
- cxxopts 3.2.1 or newer
- Qt Test module (for tests only)

On GNU/Linux and BSD, also:
- Gettext 0.22.5 or newer
- Qt D-Bus module
- KWindowSystem
- Qt's SVG image format plugin as a runtime dependency (usually located somewhere at /usr/lib64/qt6/plugins/imageformats/libqsvg.so)

On Windows, also:
- Windows 11 SDK is needed to build
- Minimum supported OS version to run Rtremotesf is Windows 10 1809 (October 2018 Update)

On macOS, also:
- Latest Xcode and macOS SDK versions supported by Qt (see [here](https://doc.qt.io/qt-6/macos.html))
- Minimum supported OS version to run Rtremotesf is macOS 12

### Building
```sh
cmake -S /path/to/sources -B /path/to/build/directory --preset base-multi
cmake --build /path/to/build/directory --config Debug
cmake --install /path/to/build/directory --config Debug --prefix /path/to/install/directory
```
This example uses base-multi preset in CMakePresets.json and Ninja Multi-Config generator.
You can invoke CMake in a different way if you want.

### Nix
A flake is provided with the package and a development shell:

```sh
nix build
nix run
nix develop
```

The package is exposed for every system listed in `lib.systems.flakeExposed`.

### Windows
Minimum supported OS version to run Rtremotesf is Windows 10 1809 (October 2018 Update).

Build instructions for MSVC toolchain with vcpkg:
1. Install Visual Studio with 'Desktop development with C++' workload
2. Install latest version of CMake (from cmake.org or Visual Studio installer)
3. Install and setup [vcpkg](https://github.com/microsoft/vcpkg#quick-start-windows), and make sure that you have 15 GB of free space on disk where vcpkg is located
4. Set VCPKG_ROOT environment variable to the location of vcpkg installation

When building from Visual Studio GUI, make sure to select 'Windows Debug' or 'Windows Release' configure preset.
Otherwise:
Launch x64 Command Prompt for Visual Studio, execute:
```pwsh
cmake -S path\to\sources -B path\to\build\directory --preset <windows-debug or windows-release>
# Initial compilation of dependencies will take a while
cmake --build path\to\build\directory
cmake --install path\to\build\directory --prefix path\to\install\directory
# Next command creates ZIP archive and MSI installer
cmake --build path\to\build\directory --target package
```

### macOS
Minimum supported OS version to run Rtremotesf is macOS 12.

Build instructions with vcpkg:
1. Install Xcode
2. Install CMake
3. Install and setup [vcpkg](https://github.com/microsoft/vcpkg#quick-start-windows), and make sure that you have 15 GB of free space on disk where vcpkg is located
4. Set VCPKG_ROOT environment variable to the location of vcpkg installation
5. Launch terminal, execute:
```sh
cmake -S path/to/sources -B path/to/build/directory --preset <macos-arm64-vcpkg or macos-x86_64-vcpkg>
# Initial compilation of dependencies will take a while
cmake --build path/to/build/directory
cmake --install path/to/build/directory --prefix path/to/install/directory
# Next command creates DMG image
cmake --build path/to/build/directory --target package
```
