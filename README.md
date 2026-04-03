# UNIX Reverse Shell
This program is a minimal, fully functional UNIX reverse shell
implementation written in C. It demonstrates how to establish a
TCP connection between a client and a listener, create an interactive
pseudo-terminal (PTY), and relay input/output between the remote
system and operator. This project includes the reverse shell
client program and the listening program, with support for
features such as raw mode handling and window size changing.

## LEGAL DISCLAIMER
This project is intended for educational purposes and authorized
security research only. Any use of this software for unauthorized
access or malicious activity is strictly prohibited. You are
responsible for complying with all applicable laws and obtaining
proper authorization before use.

## Building
This program is designed to run on Linux and macOS systems.
On Windows, it can be used through compatibility layers such as
[MSYS2](https://www.msys2.org/) or [Cygwin](https://cygwin.com/).
Here are the build instructions for each operating system:

- [Linux](BUILD_LINUX.md)
- [macOS](BUILD_MACOS.md)
- [Windows](BUILD_WINDOWS.md)
