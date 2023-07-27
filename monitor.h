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

config* getConfig(const char* filename);

int monitorDirectory(dirConfig* config, bool confirm);

void freeDirConfig(dirConfig* config);