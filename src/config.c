#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "strings.h"

struct Config *create_config() {
    struct Config *config = malloc(sizeof(struct Config));
    if (config == NULL) {
        perror("config malloc failed");
        return NULL;
    }

    config->host_name = NULL;
    config->port_number = 0;
    config->game_type = NULL;

    return config;
}

int read_config(char *config_file_name, struct Config *config) {
    FILE *file;
    file = fopen(config_file_name, "r"); //r = read mode

    if (file == NULL) { //fopen gibt 0 zurück falls file nicht existiert
        int err = errno;
        perror("could not open config file");

        if (err == ENOENT) {
            return CONFIG_FILE_NOT_EXISTS;
        }

        return CONFIG_FILE_NOT_READABLE;
    }

    char content[MAX_CONFIG_FILE_SIZE+1]; // we save one extra byte for the terminating null byte
    long unsigned int i = 0;
    long unsigned int line_breaks = 0;

    //Liest komplettes file
    while (i < sizeof(content) && (content[i] = fgetc(file)) != EOF) {
        //zählt alle Zeilen also \n im file
        if (content[i] == '\n') {
            line_breaks++;
        }
        i++;
    }

    fclose(file);

    if (content[i] != EOF) {
        printf("Config file is too large! (more than %d bytes)\n", MAX_CONFIG_FILE_SIZE);
        return CONFIG_FILE_TOO_LARGE;
    }
    
    content[i] = '\0';

    if (i == 0 || (i == line_breaks)) {
        printf("Config file is empty.\n");
        return CONFIG_FILE_EMPTY;
    }

    bool host_found = false;
    bool port_found = false;
    bool game_found = false;

    char *content_save_ptr;
    char *line = strtok_r(content, "\n", &content_save_ptr);

    //Ueberpruft ob "substring" host, port, game vorhanden, damit später
    //an richtigen stelle im struct gespeichert wird
    //-> Zeilen könne in der configfile untereinander beliebig vertauscht werden
    while (line != NULL) {
        char *key = strtok(line, " =");
        char *value = strtok(NULL, " =");
        if (key == NULL || value == NULL) {
            printf("Could not parse config line: '%s'\n", line);
        } else {
            if (strcasecmp(key, "host") == 0) {
                host_found = true;
                config->host_name = strdup(value);
                if (config->host_name == NULL) {
                    perror("strdup for host_name failed");
                    return CONFIG_FILE_ERROR;
                }
            } else if (strcasecmp(key, "port") == 0) {
                port_found = true;
                config->port_number = atoi(value);
            } else if (strcasecmp(key, "game") == 0) {
                game_found = true;
                config->game_type = strdup(value);
                if (config->game_type == NULL) {
                    perror("strdup for game_type failed");
                    return CONFIG_FILE_ERROR;
                }
            }
        }

        line = strtok_r(NULL, "\n", &content_save_ptr);
    }

    if (!host_found || !port_found || !game_found) {
        printf("Missing specification of either host, port or game. Check your config file.\n");
        return CONFIG_FILE_INCOMPLETE;
    }

    printf("Config: host = %s, port = %i, game = %s\n", config->host_name, config->port_number, config->game_type);

    return 0;

}

int read_default_config(struct Config *config) {
    //schreibt Konstanten in config

    config->host_name = strdup(HOSTNAME);
    if (config->host_name == NULL) {
        perror("strdup for game_type failed");
        return -1;
    }

    config->port_number = PORTNUMBER;

    config->game_type = strdup(GAMEKINDNAME);
    if (config->game_type == NULL) {
        perror("strdup for game_type failed");
        return -1;
    }

    return 0;
}

int save_config(char *config_file_name, struct Config *config) {
    FILE *file;
    file = fopen(config_file_name, "w"); //w -> zum schreiben geöffnet und erzeugt wenn nötig
    if (file == NULL) {
        perror("Error opening config file");
        return -1;
    }

    if (fprintf(file, "host = %s\nport = %d\ngame = %s\n", config->host_name, config->port_number, config->game_type) < 0) {
        printf("Error writing to config file (fprintf)\n");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

void free_config(struct Config *config) {
    if (config->host_name != NULL) {
        free(config->host_name);
        config->host_name = NULL;
    }
    if (config->game_type != NULL) {
        free(config->game_type);
        config->game_type = NULL;
    }
    free(config);
}
