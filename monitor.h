#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

//A part of the main config
typedef struct dirConfig{
    char* dirName; //Absolute, resolved path to directory
    char** whitelist; //Array of strings representing the whitelist
    size_t whitelistLen;
    char* move; //if NULL move is disabled
    bool recursive;
    bool verbose;
} dirConfig;

//Main config
typedef struct config{
    dirConfig* partConfigs;
    size_t len;
    //Global settings
    bool recursive;
    bool verbose;
    char* move; //Absolute resolved path to directory where to move the non compliant files, or NULL
} config;

/*!
 @brief Parses the config file, and allocates necessary memory for pointers.
 @param fileContents the config contents
 @return a newly created config, or NULL on error 
*/
config* getConfig(char* fileContents);

/*!
 @brief Parses the config file
 @param filename the config filename, resolved or not
 @return the newly created config, or NULL on error
*/
config* getConfigFilename(const char* filename);

/*!
 @brief Does a directory sweep according to the provided config
 @param config the config that determines the behaviour of this sweep
 @param confirm toggle for action confirm
 @param recursive override for recursive behaviour
 @param verbose override for logging of all actions
 @param move override for move; NULL, or path to the directory where non compliant files should be moved
 @return the number of affected files, or negative value in case of error
*/
int monitorDirectory(dirConfig* config, bool confirm, bool recursive, bool verbose, char* move);

/*!
 @brief frees a dirConfig
*/
void freeDirConfig(dirConfig* config);