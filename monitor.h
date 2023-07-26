#include <stdlib.h>
#include <stdbool.h>

typedef struct dirConfig{
    char* dirName;
    char** whitelist;
    size_t whitelistLen;
    char* move;
    bool recursive;
    bool verbose;
    bool confirm;
} dirConfig;

typedef struct config{
    dirConfig* partConfigs;
    size_t len;
} config;

config getConfig(char* filename);

int monitorDirectory(dirConfig config);

void freeDirConfig(dirConfig config);