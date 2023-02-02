#define _GNU_SOURCE

#include <poll.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "net.h"
#include "thinker.h"
#include "shm.h"

// Read message from server and compare it against given message.
// 
// message: Null-terminated string
// 
// Returns 0 if received message matches expected one, -1 otherwise.
static int client_expect_message(struct Client *client, char *message);

// Read message from server and compare it against given regex.
// It's best to use a regex that matches the whole message: "^\\+ ...$" (^ means start of the message, $ means end of the message).
// 
// regex: Regex pattern as null-terminated string
// nmatch: Number of matches, which is 1 + number of opening parentheses
// pmatch: Array of size nmatch, to store the matches
// 
// Returns 0 if received message matches regex (and regex compiles), -1 otherwise.
static int client_expect_message_regex(struct Client *client, char *regex, size_t nmatch, regmatch_t *pmatch);

// Only check the current server message against given regex, without receiving a new message.
// It's best to use a regex that matches the whole message: "^\\+ ...$" (^ means start of the message, $ means end of the message).
// 
// regex: Regex pattern as null-terminated string
// nmatch: Number of matches, which is 1 + number of opening parentheses
// pmatch: Array of size nmatch, to store the matches
// 
// Returns 0 if current message matches regex (and regex compiles), -1 if the regex does not compile and -2 if the message does not match.
static int client_check_message_regex(struct Client *client, char *regex, size_t nmatch, regmatch_t *pmatch);

// Copies the matched substring of text into a new string, which is automatically malloced with the correct size.
//
// text: The whole string on which the regex was executed
// match: The match, e.g. pmatch[1] if the first matching group (parentheses) is needed
//
// Returns pointer to null-terminated string, containing the match text. Must be freed!
static char *malloc_regex_match(char *text, regmatch_t match);

// Expects the field information from server, starting with FIELD and ending with ENDFIELD.
// Stores field in shared memory.
//
// Returns 0 if reading field succeeded, -1 otherwise.
static int client_expect_field(struct Client *client);


struct Client *client_create(struct Net *net, struct SharedMemory *shared_memory, int thinker_pipe_fd) {
    struct Client *client = malloc(sizeof(struct Client));
    if (client == NULL) {
        perror("client malloc failed");
        return NULL;
    }

    client->net = net;
    client->shared_memory = shared_memory;
    client->thinker_pipe_fd = thinker_pipe_fd;
    return client;
}

int client_play(struct Client *client, char *game_id, int desired_player_nr) {
    regmatch_t pmatch2[2];
    regmatch_t pmatch3[3];
    regmatch_t pmatch4[4];

    if (client_expect_message_regex(client, "^\\+ MNM Gameserver v([0-9.]+) accepting connections$", 2, pmatch2) != 0) {
        return -1;
    }
    regmatch_t version_match = pmatch2[1];
    if (client->net->message[version_match.rm_so] != '2' || client->net->message[version_match.rm_so+1] != '.') {
        printf("Unsupported server version: %.*s\n", version_match.rm_eo - version_match.rm_so, client->net->message + version_match.rm_so);
        return -1;
    }

    if (net_sendline(client->net, "VERSION 2.0", 11) != 0) {
        return -1;
    }

    if (client_expect_message(client, "+ Client version accepted - please send Game-ID to join") != 0) {
        return -1;
    }

    char dest[17] = "ID ";
    strcat(dest, game_id);
    if (net_sendline(client->net, dest, 16) != 0) {
        return -1;
    }

    if (client_expect_message(client, "+ PLAYING Quarto") != 0) {
        return -1;
    }

    if (client_expect_message_regex(client, "^\\+ (.+)$", 2, pmatch2) != 0) {
        return -1;
    }
    char *game_name = malloc_regex_match(client->net->message, pmatch2[1]);
    printf("Current Game-Name is: '%s'\n", game_name); // TODO: do something better with the game name than just printing it
    free(game_name);
    game_name = NULL;

    if (desired_player_nr != -1) {
        char *message = NULL;
        if (asprintf(&message, "PLAYER %d", desired_player_nr) == -1) {
            perror("Failure during building PLAYER message");
            return -1;
        } else if (net_sendline(client->net, message, strlen(message)) != 0) {
            free(message);
            return -1;
        }
        free(message);
    } else {
        if (net_sendline(client->net, "PLAYER", 6) != 0) {
            return -1;
        }
    }

    if (client_expect_message_regex(client, "^\\+ YOU ([0-9]{1,5}) (.+)$", 3, pmatch3) != 0) {
        return -1;
    }

    char *player_nr_str = malloc_regex_match(client->net->message, pmatch3[1]);
    int player_nr = atoi(player_nr_str); // atoi is a bad choice if the string number is out of int range, but here, we're sure that it's in range
    client->shared_memory->player_nr = player_nr;
    free(player_nr_str);
    player_nr_str = NULL;
    char *player_name = malloc_regex_match(client->net->message, pmatch3[2]);
    printf("Were playing with player #%d: '%s'\n", player_nr, player_name);
    shm_set_player_name(client->shared_memory, player_name);


    if (client_expect_message_regex(client, "^\\+ TOTAL ([0-9]{1,5})$", 2, pmatch2) != 0) {
        free(player_name);
        return -1;
    }
    char *player_count_str = malloc_regex_match(client->net->message, pmatch2[1]);
    int player_count = atoi(player_count_str); // atoi is a bad choice if the string number is out of int range, but here, we're sure that it's in range
    free(player_count_str);
    player_count_str = NULL;

    struct PlayerData *players = malloc(sizeof(struct PlayerData) * player_count);

    players[0].player_name = player_name;
    players[0].player_nr = player_nr;
    players[0].ready = true;

    for (int i = 1; i < player_count; i++) {
        if (client_expect_message_regex(client, "^\\+ ([0-9]{1,5}) (.+) (0|1)$", 4, pmatch4) != 0) {
            for (int j = 0; j < i; j++) {
                free(players[j].player_name);
            }
            free(players);
            return -1;
        }

        char *other_player_nr_str = malloc_regex_match(client->net->message, pmatch4[1]);
        int other_player_nr = atoi(other_player_nr_str); // atoi is a bad choice if the string number is out of int range, but here, we're sure that it's in range
        players[i].player_nr = other_player_nr;
        free(other_player_nr_str);
        other_player_nr_str = NULL;

        char *other_player_name = malloc_regex_match(client->net->message, pmatch4[2]);
        players[i].player_name = other_player_name;

        bool other_player_ready = client->net->message[pmatch4->rm_so] == '1';
        players[i].ready = other_player_ready;
    }

    int ret_shm_set_players = shm_set_players(client->shared_memory, players, player_count);
    for (int i = 0; i < player_count; i++) {
        free(players[i].player_name);
    }
    player_name = NULL;
    free(players);
    players = NULL;
    if (ret_shm_set_players != 0) {
        return -1;
    }

    if (client_expect_message(client, "+ ENDPLAYERS") != 0) {
        return -1;
    }


    while(true) {
        if (net_recvline(client->net) <= 0) {
            return -1;
        }

        if (strcmp(client->net->message, "+ WAIT") == 0) {

            char *wait_response = "OKWAIT";
            if (net_sendline(client->net, wait_response, strlen(wait_response)) != 0) {
                return -1;
            }

        } else if (client_check_message_regex(client, "^\\+ MOVE ([0-9]{1,6})$", 2, pmatch2) == 0) {
            char *timeout_str = malloc_regex_match(client->net->message, pmatch2[1]);
            client->shared_memory->move_timeout = atoi(timeout_str);
            free(timeout_str);
            timeout_str = NULL;

            if (client_expect_message_regex(client, "^\\+ NEXT ([0-9]{1,3})$", 2, pmatch2) != 0) {
                return -1;
            }
            char *block_str = malloc_regex_match(client->net->message, pmatch2[1]);
            client->shared_memory->move_block_nr = atoi(block_str);
            free(block_str);
            block_str = NULL;

            if (client_expect_field(client) != 0) {
                return -1;
            }

            char *thinking_message = "THINKING";
            if (net_sendline(client->net, thinking_message, strlen(thinking_message)) != 0) {
                return -1;
            }

            if (client_expect_message(client, "+ OKTHINK") != 0) {
                return -1;
            }

            printf("Thinker PID: %d\n", client->shared_memory->thinker_pid);
            printf("Connector PID: %d\n", client->shared_memory->connector_pid);

            client->shared_memory->thinker_request = true;

            if (kill(client->shared_memory->thinker_pid, SIGUSR1) != 0) {
                perror("Failed sending signal to thinker");
                return -1;
            }

            struct Move move;
            int move_data_received = 0;

            while (true) {
                struct pollfd pipe_has_data;
                pipe_has_data.fd = client->thinker_pipe_fd;
                pipe_has_data.events = POLLIN;

                if (poll(&pipe_has_data, 1, 0) == 1) { // we use timeout of 0 to check if pipe has data right now
                    char buf[sizeof(move)];
                    int n = read(client->thinker_pipe_fd, buf, sizeof(move));
                    if (n == -1) {
                        perror("Failure during reading message from pipe");
                        return -1;
                    }
                    if (n == 0) {
                        printf("Failure during reading message from pipe: EOF\n");
                        return -1;
                    }

                    if (move_data_received + n > (int)sizeof(move)) {
                        printf("Failure during reading message from pipe: Too much data (move_data_received = %d, n = %d)\n", move_data_received, n);
                        return -1;
                    }

                    memcpy((char *)&move + move_data_received, buf, n);
                    move_data_received += n;

                    if (move_data_received == sizeof(move)) {
                        break;
                    } else {
                        printf("Received partial data from Thinker (%d/%ld bytes)\n", move_data_received, sizeof(move));
                    }
                } else if (net_has_data(client->net)) {
                    printf("While waiting for Thinker, received data from server.\n");

                    if (net_recvline(client->net) <= 0) {
                        return -1;
                    }

                    printf("Unexpected server message: '%s'\n", client->net->message);
                    return -1;
                } else {
                    usleep(100); // 100µs
                }
            }

            char *play_message = NULL;
            int asprintf_ret;
            if (move.next_block_nr < 0) {
                asprintf_ret = asprintf(&play_message, "PLAY %c%d", 'A' + move.x, 1 + move.y);
            } else {
                asprintf_ret = asprintf(&play_message, "PLAY %c%d,%d", 'A' + move.x, 1 + move.y, move.next_block_nr);
            }
            if (asprintf_ret == -1) {
                perror("Failure during building PLAY message");
                return -1;
            }
            if (net_sendline(client->net, play_message, strlen(play_message)) != 0) {
                free(play_message);
                return -1;
            }
            free(play_message);
            play_message = NULL;

            if (client_expect_message(client, "+ MOVEOK") != 0) {
                return -1;
            }
        } else if (strcmp(client->net->message, "+ GAMEOVER") == 0) {

            if (client_expect_field(client) != 0) {
                return -1;
            }

            if (client_expect_message_regex(client, "^\\+ PLAYER0WON (Yes|No)$", 2, pmatch2) != 0) {
                return -1;
            }
            char *player0_status = malloc_regex_match(client->net->message, pmatch2[1]);

            if (client_expect_message_regex(client, "^\\+ PLAYER1WON (Yes|No)$", 2, pmatch2) != 0) {
                free(player0_status);
                return -1;
            }
            char *player1_status = malloc_regex_match(client->net->message, pmatch2[1]);

            if (strcmp(player0_status, player1_status) == 0) {
                printf("Game result: Tie!\n");
            } else if ((player_nr == 0 && strcmp(player0_status, "Yes") == 0) || (player_nr == 1 && strcmp(player1_status, "Yes") == 0)) {
                printf("Game result: Our AI has won!\n");
            } else {
                printf("Game result: Our AI lost!\n");
            }

            free(player0_status);
            player0_status = NULL;
            free(player1_status);
            player1_status = NULL;

            if (client_expect_message(client, "+ QUIT") != 0) {
                return -1;
            }

            return 0;
        } else {
            printf("Unexpected server response during game loop: '%s'\n", client->net->message);
            return -1;
        }
    }
}

static int client_expect_field(struct Client *client) {
    regmatch_t pmatch2[2];
    regmatch_t pmatch3[3];

    if (client_expect_message_regex(client, "^\\+ FIELD ([0-9]{1,2}),([0-9]{1,2})$", 3, pmatch3) != 0) {
        return -1;
    }

    char *width_string = malloc_regex_match(client->net->message, pmatch3[1]);
    int width = atoi(width_string);
    free(width_string);
    width_string = NULL;
    char *height_string = malloc_regex_match(client->net->message, pmatch3[2]);
    int height = atoi(height_string);
    free(height_string);
    height_string = NULL;

    if (width != height) {
        printf("Only square fields are allowed, but width = %d, height = %d\n", width, height);
        return -1;
    }

    int size = width;

    int field[size*size];

    for (int y = size -1; y >= 0; y--) {
        char *regex = NULL;
        if (asprintf(&regex, "^\\+ %d ((([0-9]{1,3}|\\*) ?){%d})$", y+1, size) == -1) {
            perror("Failure during building field parsing regex");
            return -1;
        } else if (client_expect_message_regex(client, regex, 2, pmatch2) != 0) {
            free(regex);
            return -1;
        }

        free(regex);
        regex = NULL;

        char *delimiter = " ";
        char *block_str = strtok(&client->net->message[pmatch2[1].rm_so], delimiter);
        for (int x = 0; x < size; x++) {
            if (block_str == NULL) {
                printf("Invalid format!\n");
                return -1;
            }

            if (strcmp(block_str, "*") == 0) {
                field[y*size+x] = -1;
            } else {
                field[y*size+x] = atoi(block_str);
            }

            block_str = strtok(NULL, delimiter);
        }
    }

    if (client_expect_message(client, "+ ENDFIELD") != 0) {
        return -1;
    }

    shm_set_field(client->shared_memory, field, size);

    return 0;
}

static int client_expect_message(struct Client *client, char *message) {
    if (net_recvline(client->net) <= 0) {
        return -1;
    }

    if (strcmp(client->net->message, message) != 0) {
        printf("Unexpected server response: '%s', expected: '%s'\n", client->net->message, message);
        return -1;
    }

    return 0;
}

static int client_expect_message_regex(struct Client *client, char *regex, size_t nmatch, regmatch_t *pmatch) {
    if (net_recvline(client->net) <= 0) {
        return -1;
    }

    if (client_check_message_regex(client, regex, nmatch, pmatch) == -2) {
        printf("Unexpected server response: '%s', expected regex: '%s'\n", client->net->message, regex);
        return -1;
    }

    return 0;
}

static int client_check_message_regex(struct Client *client, char *regex, size_t nmatch, regmatch_t *pmatch) {
    regex_t preg;
    int res = regcomp(&preg, regex, REG_EXTENDED);
    if (res != 0) {
        char errbuf[256];
        regerror(res, &preg, errbuf, 256);
        printf("Error compiling regex '%s': %s\n", regex, errbuf);
        return -1;
    }

    int ret = regexec(&preg, client->net->message, nmatch, pmatch, 0);
    regfree(&preg);
    if (ret == REG_NOMATCH) {
        return -2;
    }

    return 0;
}

static char *malloc_regex_match(char *text, regmatch_t match) {
    int match_len = match.rm_eo - match.rm_so;
    char *match_text = malloc((match_len + 1) * sizeof(char));
    memcpy(match_text, text + match.rm_so, match_len);
    match_text[match_len] = '\0';
    return match_text;
}
