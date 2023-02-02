#ifndef net_h
#define net_h

#include <stdbool.h>

#define NET_BUFFER_SIZE 256

struct Net {
    int sockfd;
    char buffer[NET_BUFFER_SIZE];
    int n_leftover;
    char message[NET_BUFFER_SIZE];
};

struct Net *net_create();

// Cleanup of net's fields and free net.
void net_free(struct Net *net);

// Create socket and connect to given hostname and port.
//
// Returns 0 on success, -1 otherwise
int net_connect(struct Net *net, char *hostname, int port);

// Receive a newline-terminated message from server.
// Message is written to net->message.
//
// Returns message length on success, -1 otherwise.
int net_recvline(struct Net *net);

// Returns whether there is new (unread) data from the server, i.e. whether leftover data is not empty or socket has data.
bool net_has_data(struct Net *net);

// Send a message (newline is automatically appended).
// msg: message (without newline) to be sent.
// n: message length in bytes (i.e. number of characters without possible terminating null character).
//
// Returns 0 on success, -1 otherwise.
int net_sendline(struct Net *net, char *msg, int n);

#endif
