#define _GNU_SOURCE //for func: strcasestr (must be at beginning of file)
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "config.h"
#include "main.h"
#include "net.h"
#include "shm.h"
#include "thinker.h"

int main(int argc, char **argv) {
    int ret_val = EXIT_SUCCESS;

    int checkOptionG = 0;
    int checkOptionP = 0;
    char *game_id = NULL;
    int player_nr = -1;
    struct Config *config = NULL;

    // TODO by Skruppy: argv[0] enthält den namen des Programms, i könnte bei 1 anfangen. aber man kann sich die prüfung hier auch ganz schenken, da man einfach nach der getopt() schleife schauen kann ob `game_id == NULL` ist. Die `-p` muss sogar fehlen dürfen. Das soll ein optionales argument sein (im gegensatzt zur game ID, die ein verpflichtendes argument sein soll). Fehlt die soll der gameserver den nächsten freien spieler zuweisen.
    for (int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "-g") == 0) {
            checkOptionG = 1;
        }
        if(strcmp(argv[i], "-p") == 0) {
            checkOptionP = 1;
        }
    }

    if(checkOptionG == 0 && checkOptionP == 0) {
        printf("Game-ID missing! Use option \"-g\"\n");
        printf("Player amount missing! Use option \"-p\"\n");
        return EXIT_FAILURE;
    }
    if(checkOptionG == 0) {
        printf("Game-ID missing! Use option \"-g\"\n");
        return EXIT_FAILURE;
    }
    if(checkOptionP == 0) {
        printf("Player amount missing! Use option \"-p\"\n");
        return EXIT_FAILURE;
    }


    //für options: -g <Game-ID> und -p <1,2>
    opterr = true;
    int opt;
    while((opt = getopt(argc, argv, "g:p:")) != -1) {
        switch (opt) {
            case 'g':
                if (strlen(optarg) != GAME_ID_LENGTH) {
                    printf("Game-ID invalid! Make sure it has %d digits!\n", GAME_ID_LENGTH);
                    return EXIT_FAILURE;
                }
                game_id = optarg;
                break;

            case 'p':
                if (atoi(optarg) <= 0) {
                    printf("Select a positive number as player number!\n");
                    return EXIT_FAILURE;
                }
                player_nr = atoi(optarg) - 1; // player numbers are zero-indexed, even though the UI shows them as one-indexed
                break;

            default:
                return EXIT_FAILURE; // missing or additional argument

        }
    }

    char *config_path;
    if (argc < 6) { // TODO by Skruppy: Mit optionalen argumenten klappt das nicht mehr. man könnte z.B. in der schleife drüber das optionale argument `-c` prüfen (und `config_path` simpel mit "client.conf" initialisieren). Generess sind das hier und die `5` ein paar zeilen weiter unten unschöne magic numbers
        printf("No config file specified, using client.conf.\n");
        config_path = "client.conf";
    } else {
        printf("Using config file %s\n", argv[5]);
        config_path = argv[5];
    }

    config = create_config();
    if (config == NULL) {
        goto error;
    }

    int read_config_ret = read_config(config_path, config);
    if (read_config_ret == CONFIG_FILE_NOT_EXISTS || read_config_ret == CONFIG_FILE_EMPTY) {
        printf("Config file is empty or doesn't exist, using default config.\n");
        read_default_config(config);

        if (save_config(config_path, config) != 0) {
            printf("Unable to save config file with default options.\n");
            goto error;
        }
    } else if (read_config_ret != 0) {
        goto error;
    }

    //FORK Setup
    int fd[2];

    //Creating Shared Memory
    int shm_id = create_shm_segment(sizeof(struct SharedMemory));
    if (shm_id < 0) {
        goto error;
    }

    printf("Shared Memory ID: %d\n", shm_id);

    //Attaching Shared Memory
    struct SharedMemory *shared_memory = shm_attach(shm_id);
    if (shared_memory == NULL) {
        goto error;
    }

    if (pipe(fd) < 0) {
        perror("Error while creating Pipe.");
        goto error;
    }

    //Forking process
    pid_t thinker_pid = getpid();
    pid_t connector_pid = fork();
    if (connector_pid < 0) {
        perror("Error with fork().");
        goto error;
    } else if (connector_pid > 0) {
        // -----Parent Process----- --> THINKER
        printf("Thinker process begin\n");

        shared_memory->thinker_pid = thinker_pid;
        shared_memory->connector_pid = connector_pid;

        // Close reading side of pipe as it's not needed
        close(fd[0]);

        struct Thinker *thinker = thinker_create(shared_memory, fd[1]);
        if (thinker == NULL) {
            goto thinker_error;
        }

        if (thinker_loop(thinker) != 0) {
            if (kill(connector_pid, SIGTERM) != 0) {
                perror("Error terminating child process");
                goto thinker_error;
            }

            wait_with_retry(connector_pid);
            goto thinker_error;
        };

        // Wait for Child Process
        if (wait_with_retry(connector_pid) == -1) {
            goto thinker_error;
        }

        goto thinker_cleanup;

        thinker_error:
        ret_val = EXIT_FAILURE;

        thinker_cleanup:
        if (thinker != NULL) {
            free(thinker);
        }
    } else {
        // -----Child Process----- --> CONNECTOR
        connector_pid = getpid();
        printf("Connector process begin\n");

        struct Net *net = NULL;
        struct Client *client = NULL;

        // Close writing side of pipe as it's not needed
        close(fd[1]);

        net = net_create();
        if (net == NULL) {
            goto error_client;
        }

        if (net_connect(net, config->host_name, config->port_number) != 0) {
            printf("Connecting failed.\n");
            goto error_client;
        };

        //Calling Connector Function
        client = client_create(net, shared_memory, fd[0]);
        if (client == NULL) {
            goto error_client;
        }

        if (client_play(client, game_id, player_nr) != 0) {
            printf("Failure during playing!\n");
            goto error_client;
        }

        goto cleanup_client;

        error_client:
        ret_val = EXIT_FAILURE;

        cleanup_client:
        if (client != NULL) {
            free(client);
            client = NULL;
        }
        if (net != NULL) {
            net_free(net);
            net = NULL;
        }
    }

    goto cleanup;

    error:
    ret_val = EXIT_FAILURE;

    cleanup:
    if (config != NULL) {
        free_config(config);
        config = NULL;
    }

    printf("Process %d stopped with exit code %d.\n", getpid(), ret_val);
    return ret_val;
}

int wait_with_retry(pid_t pid) {
    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            perror("Error waiting for Child Process");
            return -1;
        }
    }
    return 0;
}
