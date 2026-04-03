# Building on macOS
To build the reverse shell on macOS, you will need a C/C++ toolchain.
On macOS, this is called the Xcode Command Line tools. To install
the tools, run the following command:
```bash
xcode-select --install
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

