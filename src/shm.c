#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/shm.h>

#include "shm.h"


#define SHM_KEY IPC_PRIVATE
#define SHM_FLAG 0644










//size_t: is an unsigned integer type of at least 16 bit
//neue shm-segment anlegen mit groeße des structs (sizeof...)
int create_shm_segment(size_t size) {
    int shm_id;
    if ((shm_id = shmget(SHM_KEY, size, SHM_FLAG)) < 0) {
        perror("Shared Memory creation error");
        return -1;
    }
    return shm_id;
}

//Segment an Adressraum des Erzeugerprozesses anhaengen
void *shm_attach(int shm_id) {
    if (shm_id == 0) {
        printf("Given shm_id is 0\n");
        return NULL;
    }

    void *pointer;
    if ((pointer = shmat(shm_id, NULL, 0)) == NULL) {
        perror("failed to attach shm");
        return NULL;
    }

    // mark segment immediately as ready to remove, since we already attached it
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("failed to remove shared memory segment");
        return NULL;
    }

    //returntype: Pointer auf die Anfangsadresse im Adressraum
    return pointer;
}


int shm_remove_segment(int shm_id) {
    int shm_ch;
    if((shm_ch = shmctl(shm_id, IPC_RMID, NULL)) < 0) {
        perror("failed to remove shared memory segment");
        return -1;
    }
    return 0;
}

int shm_set_player_name(struct SharedMemory *shared_memory, char *player_name) {
    int n = (strlen(player_name)+1);

    int shm_id = shmget(SHM_KEY, n*sizeof(char), SHM_FLAG);
    if (shm_id == -1) {
        perror("Shared Memory creation error");
        return -1;
    }

    printf("Created player_name shm segment %d\n", shm_id);

    char *player_name_shm = shm_attach(shm_id);
    if (player_name_shm == NULL) {
        return -1;
    }

    memcpy(player_name_shm, player_name, n);
    shared_memory->player_name_shm_id = shm_id;

    return 0;
}

char *shm_get_player_name(struct SharedMemory *shared_memory) {
    return shm_attach(shared_memory->player_name_shm_id);
}

int shm_set_players(struct SharedMemory *shared_memory, struct PlayerData *players, int total_player_count) {
    size_t size = sizeof(struct PlayerDataInternal) * total_player_count;

    int shm_id = shmget(SHM_KEY, size, SHM_FLAG);
    if (shm_id == -1) {
        perror("Shared Memory creation error");
        return -1;
    }

    printf("Created players shm segment %d\n", shm_id);

    struct PlayerDataInternal *players_internal = shm_attach(shm_id);
    if (players_internal == NULL) {
        return -1;
    }

    for (int i = 0; i < total_player_count; i++) {
        int player_name_n = strlen(players[i].player_name) + 1;
        int player_name_shm_id = shmget(SHM_KEY, player_name_n*sizeof(char), SHM_FLAG);
        if (player_name_shm_id == -1) {
            perror("Shared Memory creation error");
            return -1;
        }

        printf("Created players[%d].player_name shm segment %d\n", i, player_name_shm_id);

        char *player_name_shm = shm_attach(player_name_shm_id);
        if (player_name_shm == NULL) {
            return -1;
        }

        memcpy(player_name_shm, players[i].player_name, player_name_n);

        players_internal[i].player_nr = players[i].player_nr;
        players_internal[i].player_name_shm_id = player_name_shm_id;
        players_internal[i].ready = players[i].ready;
    }

    shared_memory->total_player_count = total_player_count;
    shared_memory->players_shm_id = shm_id;
    return 0;
}

struct PlayerData *shm_get_players(struct SharedMemory *shared_memory) {
    struct PlayerDataInternal *players_internal = shm_attach(shared_memory->players_shm_id);
    if (players_internal == NULL) {
        return NULL;
    }

    struct PlayerData *players = malloc(sizeof(struct PlayerData) * shared_memory->total_player_count);

    for (int i = 0; i < shared_memory->total_player_count; i++) {
        players[i].player_nr = players_internal[i].player_nr;
        players[i].player_name = shm_attach(players_internal[i].player_name_shm_id);
        players[i].ready = players_internal[i].ready;
    }

    return players;
}

int shm_set_field(struct SharedMemory *shared_memory, int *field, int field_size) {
    size_t size = sizeof(int) * (field_size*field_size);

    int shm_id = shmget(SHM_KEY, size, SHM_FLAG);
    if (shm_id == -1) {
        perror("Shared Memory creation error for field");
        return -1;
    }

    printf("Created field shm segment %d\n", shm_id);

    char *field_shm = shm_attach(shm_id);
    if (field_shm == NULL) {
        return -1;
    }

    memcpy(field_shm, field, size);
    shared_memory->field_size = field_size;
    shared_memory->field_shm_id = shm_id;

    return 0;
}

int *shm_get_field(struct SharedMemory *shared_memory) {
    return shm_attach(shared_memory->field_shm_id);
}

//Speicheranbindung entfernen
int shm_rm(void *shm_at) {
    int shm_dt;
    if((shm_dt = shmdt(shm_at)) < 0) {
        perror("failed to remove shm");
        return EXIT_FAILURE;
    }
    return shm_dt;
}
