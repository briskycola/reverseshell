# Compiler and flags
CC = cc
CFLAGS = -std=gnu99 -Wall -Werror

# Targets
CLIENT = client
SERVER = server

# Source files
CLIENT_SRC = client.c
SERVER_SRC = server.c

# Default target
all: $(CLIENT) $(SERVER)

# Build Client
$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT)

# Build Server
$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER)

# Clean build files
clean:
	rm -f $(CLIENT) $(SERVER)
