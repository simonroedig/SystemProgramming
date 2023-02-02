#ifndef client_h
#define client_h

#include "shm.h"
#include "net.h"

struct Client {
    struct Net *net;
    struct SharedMemory *shared_memory;
    int thinker_pipe_fd;
};

// Create a new client
// 
// net: Fully created and already connected Net struct (be sure to have net_connect() called!)
// shared_memory: Shared memory
// thinker_pipe_fd: File descriptor of reading pipe to get thinker results
// 
// Returns pointer to Client which must be freed after use
struct Client *client_create(struct Net *net, struct SharedMemory *shared_memory, int thinker_pipe_fd);

// Start playing, i.e. start with the procotol
// 
// game_id: 13-character, null-terminated string
// player_nr: Desired player number; set it to -1 to choose automatically
// 
// Returns 0 on success, -1 on failure
int client_play(struct Client *client, char *game_id, int player_nr);

#endif
