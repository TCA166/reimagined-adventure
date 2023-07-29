#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

typedef struct dirConfig{
    char* dirName; //Absolute, resolved path to directory
    char** whitelist; //Array of strings representing the whitelist
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
    char* move; //Absolute resolved path to directory where to move the non compliant files, or NULL
} config;

//Parses the config file, and allocates necessary memory for pointers. Returns NULL in case of errors. Sets errno
config* getConfig(const char* filename);

//Does a directory sweep according to the provided config. Returns the number of removed/moved files or -1 in case of error. Sets errno
int monitorDirectory(dirConfig* config, bool confirm, bool recursive, bool verbose, char* move);

void freeDirConfig(dirConfig* config);