# Building on Windows
To build the reverse shell on Windows, you will need to install
[MSYS2](https://www.msys2.org/). This is a compatibility layer
that allows you to compile programs that would normally be
UNIX only on Windows.

## Installing MSYS2
To install MSYS2, refer to the instructions [here](https://www.msys2.org/).
Once you install MSYS2, you will need to use the `MSYS2 MSYS` shell.
This will be the main shell where you compile the reverse shell. Finally,
you will need the following dependencies installed:
```bash
pacman -S base-devel gcc make git
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
