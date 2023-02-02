#ifndef thinker_h
#define thinker_h

#include "shm.h"

struct Thinker {
    struct SharedMemory *shared_memory;
    int pipe_fd;
};

// Create a new thinker
// 
// shared_memory: Shared memory
// pipe_fd: File descriptor of writing pipe to write thinker results
// 
// Returns pointer to Thinker which must be freed after use
struct Thinker *thinker_create(struct SharedMemory *shared_memory, int pipe_fd);

// Start loop that responds to SIGUSR1 events by thinking and ends when the connector stops.
//
// thinker: The thinker that will be used
//
// Returns 0 on success, -1 otherwise.
int thinker_loop(struct Thinker *thinker);

// Calculate next move and write it into pipe
void thinker_think(struct Thinker *thinker);

struct Move {
    int x;
    int y;
    int next_block_nr;
};

struct Move get_best_move(int *field_array, int field_size, int block_nr);

struct Move get_random_move(int *field_array, int field_size, int block_nr);

void copyArray(int *old_array, int *new_array, int old_array_size);

int search_best_block_for_opponent(int *field_array, int fields_num , int *playable_blocks, int *playable_fields);

int find_possible_win_on_field(int *field_array, int field_size, int block_nr, int *playable_fields);

void print_array(const int *array, int array_size);

void print_board(struct Thinker *thinker);

int find_random_free_field(const int *field_array, int array_size);

int find_index (const int *array, int target, int array_size);

bool compare_line(int *line, int field_size);

bool is_winning(int *field, int field_size);

int count_elements_in_array(const int *array, int array_size);

void remove_element(int *array, int index, int array_size);

char *int_to_binary_str(int block, int field_size);

#endif

