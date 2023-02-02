#ifndef QUARTO_CLIENT_SHM_H
#define QUARTO_CLIENT_SHM_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <sys/wait.h>
#include <fcntl.h>

//Every player has at least three properties:
struct PlayerData {
    int player_nr;
    char *player_name;
    bool ready;
};

struct PlayerDataInternal {
    int player_nr;
    int player_name_shm_id;
    bool ready;
};

struct SharedMemory {
    pid_t thinker_pid;
    pid_t connector_pid;

    int player_nr;
    int player_name_shm_id;
    int total_player_count;

    int players_shm_id;

    int move_timeout;
    int move_block_nr;

    int field_size;
    int field_shm_id;

    bool thinker_request;
};

int create_shm_segment(size_t size_struct);



void *shm_attach(int shm_id);

int shm_set_player_name(struct SharedMemory *shared_memory, char *player_name);
char *shm_get_player_name(struct SharedMemory *shared_memory);

int shm_set_players(struct SharedMemory *shared_memory, struct PlayerData *players, int total_player_count);
struct PlayerData *shm_get_players(struct SharedMemory *shared_memory);

int shm_set_field(struct SharedMemory *shared_memory, int *field, int field_size);
int *shm_get_field(struct SharedMemory *shared_memory);

#endif //QUARTO_CLIENT_SHM_H
