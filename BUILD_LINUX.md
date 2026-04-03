# Building on Linux
To build the reverse shell on Linux, you will need a C/C++ toolchain.
Most Linux distributions usually have this installed but if you
don't have the toolchain, you can install it by running the following
commands depending on your Linux distribution:

## Arch Linux
```bash
sudo pacman -S base-devel
```

## Debian/Ubuntu/Linux Mint
```bash
sudo apt install build-essential
```

## Fedora/RHEL
```bash
sudo dnf install @development-tools
```

## openSUSE
```bash
sudo zypper install devel_basis
```

## Alpine Linux
```bash
doas apk add build-base
```

## Void Linux
```bash
sudo xbps-install base-devel
```

Next you will need to clone the repo and run the Makefile to build
the reverse shell:
```bash
git clone https://github.com/briskycola/reverseshell
cd reverseshell
make
```

You will see two binaries: `client` and `server`. The client is
the program that the victim will run and the server is what the
attacker will run. Note: By default, the attacker IP is configured
to be `127.0.0.1`. This makes it so that the client will connect to itself.
This allows you to test the reverse shell on the same computer with two
different terminal windows. Make sure to change the attacker IP to be the IP of your listening
computer if you plan to test this with two different computers. The port
is also `4444` by default, but you can change the port to any value from 0-65535.
