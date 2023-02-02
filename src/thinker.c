#include "thinker.h"
#include <time.h>

struct Thinker *thinker_create(struct SharedMemory *shared_memory, int pipe_fd) {
    struct Thinker *thinker = malloc(sizeof(struct Thinker));
    if (thinker == NULL) {
        perror("thinker malloc failed");
        return NULL;
    }

    thinker->shared_memory = shared_memory;
    thinker->pipe_fd = pipe_fd;
    return thinker;
}

static int last_signal = 0;
static void signal_handler(int signum) {
    last_signal = signum;
}

int thinker_loop(struct Thinker *thinker) {
    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        perror("failed setting SIGUSR1 signal handler");
        return -1;
    }
    if (signal(SIGCHLD, signal_handler) == SIG_ERR) {
        perror("failed setting SIGCHLD signal handler");
        return -1;
    }

    while (pause() == -1) {
        printf("While iteration (ready: %d)\n", thinker->shared_memory->thinker_request);

        if (last_signal == SIGCHLD) {
            printf("Stopping thinker loop since child was terminated.\n");
            return 0;
        }

        if (last_signal == SIGUSR1) {
            if (!thinker->shared_memory->thinker_request) {
                printf("Thinker requested with thinker_request = false, won't think.\n");
            } else {
                // reset thinker_request, since we're thinking now
                thinker->shared_memory->thinker_request = false;
                thinker_think(thinker);
            }
        }
    }

    return 0;
}

void thinker_think(struct Thinker *thinker) {
    //Print board
    print_board(thinker);
    char *player_name = shm_get_player_name(thinker->shared_memory);
    printf("Thinker is thinking for player '%s'...\n", player_name);

    int next_block_nr = thinker->shared_memory->move_block_nr;
    int *field = shm_get_field(thinker->shared_memory);
    int field_size = thinker->shared_memory->field_size;

    //AI move
    struct Move ai_move = get_best_move(field, field_size, next_block_nr);
    printf("Ai chose field: (%i, %i)\n", ai_move.x, ai_move.y);
    printf("Ai chose block: %i\n", ai_move.next_block_nr);

    struct Move move;
    move.x = ai_move.x;
    move.y = ai_move.y;
    move.next_block_nr = ai_move.next_block_nr;

    if (write(thinker->pipe_fd, &move, sizeof(move)) == -1) {
        perror("Error sending move into pipe\n");
        return;
    }
}

struct Move get_best_move(int *field_array, int field_size, int block_nr){
    //List of not-played fields, to know which field we can move our piece to
    int fields_num = field_size * field_size;
    int playable_fields[fields_num];
    for (int i = 0; i < fields_num; i++) {
        playable_fields[i] = i;
    }

    //Remove fields that are already played from playable_fields
    for (int i = 0; i < fields_num; i++) {
        if (field_array[i] != -1) {
            remove_element(playable_fields, i, fields_num);
        }
    }

    //List of not-played blocks, to know which block we can give to the opponent
    int playable_blocks[fields_num];
    for (int i = 0; i < fields_num; i++) {
        playable_blocks[i] = i;
    }

    //Remove the block we have to play from playable_blocks
    remove_element(playable_blocks, block_nr, fields_num);

    //Remove blocks that are already on the field
    for (int i = 0; i < fields_num; i++) {
        if (field_array[i] != -1) {
            remove_element(playable_blocks, field_array[i], fields_num);
        }
    }

    //Find winning move
    int best_field = find_possible_win_on_field(field_array, field_size, block_nr, playable_fields);
    int best_block = -1;
    if (best_field == -1) {
        best_field = find_random_free_field(field_array, fields_num);
        //No best field found. Picking random field

        //Find best block for opponent
        field_array[best_field] = block_nr;
        //Remove fields that are already played from playable_fields
        remove_element(playable_fields, best_field, fields_num);

        best_block = search_best_block_for_opponent(field_array, field_size, playable_blocks, playable_fields);
        if (best_block == -1) {
            //No best block found. Picking random block
            int blocks_num = count_elements_in_array(playable_blocks, fields_num);
            int random_block_found = (rand() % blocks_num + 1) - 1;
            best_block = playable_blocks[random_block_found];
        }
    }

    int best_field_x = best_field % field_size;
    int best_field_y = best_field / field_size;

    struct Move best_move;
    best_move.x = best_field_x;
    best_move.y = best_field_y;
    best_move.next_block_nr = best_block;

    return best_move;
}

int find_possible_win_on_field(int *field_array, int field_size, int block_nr, int *playable_fields) {
    //This function looks one move ahead and checks if there is any move that it can make that is winning
    int fields_num = field_size * field_size;
    //Iterate through all possible fields
    for (int i = 0; i < count_elements_in_array(playable_fields, fields_num); i++) {
        //Setting up new board position with new move
        int current_field_array[fields_num];
        copyArray(field_array, current_field_array, fields_num);
        int current_field = playable_fields[i];
        current_field_array[current_field] = block_nr;

        if (is_winning(current_field_array, field_size)) {
            //Winning move found
            return current_field;
        }
    }
    //No Winning move possible
    return -1;
}

int search_best_block_for_opponent(int *field_array, int field_size , int *playable_blocks, int *playable_fields) {
    // This function looks one move ahead and checks, for every block, if there is no winning position,
    // and if so, give this block
    // Also: this is happening after we DIDN'T find a "best_field"(=winning field) we play!
    for (int i = 0; i < count_elements_in_array(playable_blocks, field_size * field_size); i++) {
        int current_block = playable_blocks[i];
        if (find_possible_win_on_field(field_array, field_size, current_block, playable_fields) == -1) {
            //No move is winning with this block, so we use that one
            return current_block;
        }
    }
    return -1;
}

void copyArray(int *old_array, int *new_array, int old_array_size) {
    // loop to iterate through array
    for (int i = 0; i < old_array_size; ++i) {
        new_array[i] = old_array[i];
    }
}

void print_board(struct Thinker *thinker) {
    int field_size = thinker->shared_memory->field_size;
    int *field = shm_get_field(thinker->shared_memory);

    printf("\n\n");
    char* nextBlock = int_to_binary_str(thinker->shared_memory->move_block_nr, field_size);
    printf("We have to move block %s %d in %dms.\n", nextBlock, thinker->shared_memory->move_block_nr, thinker->shared_memory->move_timeout);
    free(nextBlock);
    printf("\n");

    for (int y = field_size - 1; y >= 0; y--) {
        printf("%2d  ", y+1);
        for (int x = 0; x < field_size; x++) {
            int block = field[y*field_size + x];
            char* binary = int_to_binary_str(block, field_size);
            printf("%s ", binary);
            free(binary);
        }
        printf("\n");
    }
    printf("   ");
    for (int x = 0; x < field_size; x++) {
        printf("  %c  ", 'A' + x);
    }
    printf("\n\n");
}

void remove_element(int *array, int element, int array_size) {
    int index = find_index(array, element, array_size);
    for (int i = index; i < array_size; i++) {
        if (i == array_size - 1){
            array[i] = -1;
        } else{
            array[i] = array[i + 1];
        }
    }
}

int find_index (const int *array, int target, int array_size) {
    int i = 0;
    while ((i < array_size) && (array[i] != target)) i++;

    return (i < array_size) ? (i) : (-1);
}

int find_random_free_field(const int *field_array, int array_size) {
    for(int i = 0; i < 1000; i++){
        srand(i);
        int random_index = (rand() % array_size + 1) - 1;
        if (field_array[random_index] != -1) {
            continue;
        }

        return random_index;
    }
    return -1;
}

bool is_winning(int *field_array, int field_size) {
    //Pointer array to contain all lines
    int *all_lines[field_size * 2 + 2];
    int *dia1 = malloc(sizeof (int) * field_size);
    int *dia2 = malloc(sizeof (int) * field_size);

    for (int i = 0; i < field_size; i++) {
        int *row = malloc(sizeof (int) * field_size);
        int *col = malloc(sizeof (int) * field_size);
        dia1[i] = field_array[i * (1 + field_size)];
        dia2[i] = field_array[field_size * (field_size - 1) - (i * (field_size - 1))];
        for (int j = 0; j < field_size; j++) {
            row[j] =  field_array[i *field_size + j];
            col[j] = field_array[j * field_size + i];
        }
        all_lines[i + 2] = row;
        all_lines[i + 2 + field_size] = col;

    }
    all_lines[0] = dia1;
    all_lines[1] = dia2;

    for (int i = 0; i < field_size * 2 + 2; i++) {
        int *line = all_lines[i];
        int count = 0;
        for (int j = 0; j < field_size; j++) {
            if (line[j] != -1) {
                count ++;
            }
        }
        if (count == field_size) {
            if(compare_line(line, field_size)){
                for (int j = 0; j < field_size * 2 + 2; j++) {
                    free(all_lines[j]);
                }
                return true;
            }
        }
    }
    for (int i = 0; i < field_size * 2 + 2; i++) {
        free(all_lines[i]);
    }

    return false;
}

bool compare_line(int *line, int field_size){
    int blocks[field_size];
    for (int i = 0; i < field_size; i++) {
        blocks[i] = line[i];
    }

    for (int i = 0; i < field_size; i++) {
        int sum = 0;
        for (int j = 0; j < field_size; j++) {
            sum += ((blocks[j] & (1<<i))>>i);
        }
        if (sum == 0 || sum == field_size) {

            return true;
        }
    }
    return false;
}

void print_array(const int *array, int array_size) {
    for (int i = 0; i < array_size; i++) {
        printf("%i: %i\n", i, array[i]);
    }
}

int count_elements_in_array(const int *array, int array_size) {
    int count = 0;
    for (int i = 0; i < array_size; i++) {
        if (array[i] != -1) {
            count++;
        }
    }
    return count;
}

char *int_to_binary_str(int block, int field_size) {
    char *binary = malloc(sizeof(char) * (field_size+1));
    binary[field_size] = '\0';

    for (int i = 0; i < field_size; i++) {
        if (block < 0) {
            binary[i] = '*';
        } else if ((block >> (field_size-i-1)) % 2 == 0) {
            binary[i] = '0';
        } else {
            binary[i] = '1';
        }
    }
    return binary;
}
