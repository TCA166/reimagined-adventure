#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "monitor.h"

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }
    char* configPath = argv[1];
    if (access(configPath, F_OK) != 0) {
        fprintf(stderr, "Config file couldn't be accessed\n");
        exit(EXIT_FAILURE);
    }
    bool confirmMode = false;
    bool verbose = false;
    bool recursive = false;
    for(int i = 2; i < argc; i++){
        if(strcmp(argv[i], "-v") == 0){
            verbose = true;
        }
        else if(strcmp(argv[i], "-c") == 0){
            confirmMode = true;
        }
        else if(strcmp(argv[i], "-r") == 0){
            recursive = true;
        }
    }
    config* mainConfig = getConfig(configPath);
    if(mainConfig == NULL){
        perror("Error encountered in getConfig");
        exit(EXIT_FAILURE);
    }
    mainConfig->verbose |= verbose;
    mainConfig->recursive |= recursive;
    printf("Config loaded\n");  
    for(int i = 0; i < mainConfig->len; i++){
        if(monitorDirectory(mainConfig->partConfigs + i, confirmMode, mainConfig->recursive, mainConfig->verbose, mainConfig->move) < 0){
            perror("Error encountered in monitorDirectory");
        }
        freeDirConfig(mainConfig->partConfigs + i);
    } 
    free(mainConfig->partConfigs);
    free(mainConfig);
    return 0;
}