#include <stdlib.h>
#include <stdbool.h>

typedef struct dirConfig{
    char* dirName;
    char** whitelist;
    size_t whitelistLen;
    char* move;
    bool recursive;
    bool verbose;
} dirConfig;

typedef struct config{
    dirConfig* partConfigs;
    size_t len;
} config;

//Parses the config file, and allocates necessary memory for pointers
config* getConfig(const char* filename);

//Does a directory sweep according to the provided config
int monitorDirectory(dirConfig* config, bool confirm);

void freeDirConfig(dirConfig* config);