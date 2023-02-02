#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

struct Net *net_create() {
    struct Net *net = malloc(sizeof(struct Net));
    if (net == NULL) {
        perror("net malloc failed");
        return NULL;
    }

    net->sockfd = 0;
    net->buffer[0] = '\0';
    net->n_leftover = 0;
    net->message[0] = '\0';
    return net;
}

void net_free(struct Net *net) {
    if (net->sockfd != 0) {
        close(net->sockfd);
        net->sockfd = 0;
    }

    free(net);
}

int net_connect(struct Net *net, char *hostname, int port) {
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        printf("Host not found: %s\n", hostname);
        return -1;
    }

    printf("Host name: %s\n", host->h_name);

    if (host->h_addr_list[0] == NULL) {
        printf("No address found!\n");
        return -1;
    }
    struct in_addr address = *(struct in_addr*)(host->h_addr_list[0]);

    struct sockaddr_in socket_address;
    memset(&socket_address, '\0', sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(port);
    socket_address.sin_addr = address;

    net->sockfd = socket(PF_INET, SOCK_STREAM, 0);

    printf("Trying to connect to IP %s at port %d ...\n", inet_ntoa(address), port);
    if (connect(net->sockfd, (struct sockaddr*) &socket_address, sizeof(struct sockaddr)) == -1) {
        // fun fact:
        // sizeof(address) == 4 und sizeof(struct sockaddr) == 16,
        // in der Beispielimplementierung von chat.c wird aber (fälschlicherweise?) sizeof(address) verwendet,
        // was hier zu einem error "invalid argument" geführt hatte.
        perror("Error");
        return -1;
    }

    printf("Successfully connected to server.\n");
    return 0;
}

int net_recvline(struct Net *net) {
    int message_length = 0;

    // We still have data from the last recv, so read the data into the message buffer.
    // Maybe the leftover data already includes a newline; in this case, we don't need another recv.
    for (int i = 0; i < net->n_leftover; i++) {
        net->message[i] = net->buffer[i];
        message_length++;

        if (net->buffer[i] == '\n') {
            // Move data after the newline to begin of the buffer
            for (int j = i + 1; j < net->n_leftover; j++) {
                net->buffer[j-(i+1)] = net->buffer[j];
            }
            net->buffer[net->n_leftover-(i+1)] = '\0';
            net->n_leftover = net->n_leftover - (i+1);

            net->message[i] = '\0';
            printf("S: %s\n", net->message);
            return message_length;
        }
    }

    // do as many recvs until we found a newline or an error occured
    while(true) {
        int n = recv(net->sockfd, net->buffer, NET_BUFFER_SIZE-1, 0); // We read size-1 bytes to keep enough space for null character

        if (n == -1) {
            perror("Error");
            return -1;
        }
        
        if (n == 0) {
            printf("Error: Connection is closed.\n");
            return -1;
        }

        for (int i = 0; i < n; i++) {
            if (message_length + 1 > NET_BUFFER_SIZE) {
                printf("Error: Message exceeds buffer size. buffer='%s', message='%s', n=%d\n", net->message, net->buffer, n);
                return -1;
            }

            net->message[message_length] = net->buffer[i];
            message_length++;

            if (net->buffer[i] == '\n') {
                // Move data after the newline to begin of the buffer
                for (int j = i + 1; j < n; j++) {
                    net->buffer[j-(i+1)] = net->buffer[j];
                }
                net->buffer[n-(i+1)] = '\0';
                net->n_leftover = n - (i+1);

                net->message[message_length-1] = '\0';
                printf("S: %s\n", net->message);
                return message_length;
            }
        }
    }

}

bool net_has_data(struct Net *net) {
    if (net->n_leftover > 0) {
        return true;
    }

    struct pollfd has_data;
    has_data.fd = net->sockfd;
    has_data.events = POLLIN;

    return poll(&has_data, 1, 0) == 1;
}

int net_sendline(struct Net *net, char *msg, int n) {
    printf("C: %s\n", msg);

    if (send(net->sockfd, msg, n, 0) != n) {
        perror("Error sending message");
        return -1;
    }

    if (send(net->sockfd, "\n", 1, 0) != 1) {
        perror("Error sending message's terminating newline character");
        return -1;
    }

    return 0;
}
