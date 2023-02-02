#ifndef config_h
#define config_h

#define _GNU_SOURCE //for func: strcasestr (must be at beginning of file)


#define GAMEKINDNAME "Quarto"
#define PORTNUMBER 1357
#define HOSTNAME "sysprak.priv.lab.nm.ifi.lmu.de"

#define CONFIG_FILE_NOT_EXISTS -1
#define CONFIG_FILE_NOT_READABLE -2
#define CONFIG_FILE_EMPTY -3
#define CONFIG_FILE_TOO_LARGE -4
#define CONFIG_FILE_INCOMPLETE -5
#define CONFIG_FILE_ERROR -6

#define MAX_CONFIG_FILE_SIZE 1024

//In struct werden ausgelesenen KonfigParameter abgelegt
struct Config {
    char *host_name;
    int port_number; //datatype int for htons()
    char *game_type; //hier: Quarto
};

// Create empty config. Must be freed. Returns null on error.
struct Config *create_config();

// Reads config file and copies data into config struct.
// Config params need to be freed afterwards (if non-null).
// Initial values in config struct are overwritten, so be sure to free them before.
//
// Returns 0 on success and error code otherwise.
int read_config(char *config_file_name, struct Config *config);

// Copies hard-coded default data into config struct.
// Config params need to be freed afterwards (if non-null).
// Initial values in config struct are overwritten, so be sure to free them before.
// 
// Returns 0 on success, -1 otherwise.
int read_default_config(struct Config *conf);

// Saves config data into config file.
// 
// Returns 0 on success, -1 otherwise.
int save_config(char *config_file_name, struct Config *config);

// Free config params (if non-null) and config struct itself.
void free_config(struct Config *config);

#endif
