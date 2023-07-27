#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

typedef struct dirConfig{
    char* dirName;
    char** whitelist;
    size_t whitelistLen;
    char* move; //if NULL move is disabled
    bool recursive;
    bool verbose;
} dirConfig;

typedef struct config{
    dirConfig* partConfigs;
    size_t len;
    //Global settings
    bool recursive;
    bool verbose;
    char* move;
} config;

//Parses the config file, and allocates necessary memory for pointers
config* getConfig(const char* filename);

//Does a directory sweep according to the provided config
int monitorDirectory(dirConfig* config, bool confirm, bool recursive, bool verbose, char* move);

void freeDirConfig(dirConfig* config);