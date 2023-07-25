#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#ifndef __unix__
    #error "Compilation on non unix systems not supported"
#endif

typedef struct dirConfig{
    char* dirName;
    char** whitelist;
    size_t whitelistLen;
} dirConfig;

typedef struct config{
    dirConfig* partConfigs;
    size_t len;
} config;

config getConfig(char* filename){
    FILE* configFile = fopen(filename, "r");
    if(configFile == NULL){
        exit(EXIT_FAILURE);
    }
    if(fseek(configFile, 0, SEEK_END) != 0){
        exit(EXIT_FAILURE);
    }
    size_t fileSize = ftell(configFile);
    if(fseek(configFile, 0, SEEK_SET) != 0){
        exit(EXIT_FAILURE);
    }
    char* fileContents = calloc(fileSize, 1);
    if(fread(fileContents, 1, fileSize, configFile) != fileSize){
        exit(EXIT_FAILURE);
    }
    config result;
    result.len = 0;
    result.partConfigs = NULL;
    dirConfig part;
    part.dirName = NULL;
    part.whitelist = NULL;
    part.whitelistLen = 0;
    char* token = strtok(fileContents, "\n");
    while(token != NULL){
        if(token[0] != '\t'){ //we are in the dir def line
            if(part.dirName != NULL){
                //append the part to the result
                result.partConfigs = realloc(result.partConfigs, (result.len + 1) * sizeof(struct dirConfig));
                result.partConfigs[result.len] = part;
                result.len++;
                //reset the part variable
                free(part.dirName);
                part.dirName = NULL;
                for(int i = 0; i < part.whitelistLen; i++){
                    free(part.whitelist[i]);
                }
                free(part.whitelist);
                part.whitelist = NULL;
                part.whitelistLen = 0;
            }
            part.dirName = malloc(strlen(token) + 1);
            strcpy(part.dirName, token);
        }
        else{ //we are in the whitelist line
            token++;
            part.whitelist = realloc(part.whitelist, (part.whitelistLen + 1) * sizeof(char*));
            part.whitelist[part.whitelistLen] = malloc(strlen(token) + 1);
            strcpy(part.whitelist[part.whitelistLen], token);
            part.whitelistLen++;
        }
    }
    return result;
}

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }
    char* config = argv[1];
    if (access(config, F_OK) != 0) {
        fprintf(stderr, "Config file couldn't be accessed\n");
        exit(EXIT_FAILURE);
    }
    pid_t pid; //our process deamon
    if((pid = fork()) > 0){ //parent process
        printf("Child process created\n");
        exit(EXIT_SUCCESS);
    }
    else if(pid < 0){ //parent process
        fprintf(stderr, "Error encountered during fork\n");
        exit(EXIT_FAILURE);
    }
    else{ //child process
        if(setsid() != pid){
            fprintf(stderr, "Error encountered during setsid\n");
            exit(EXIT_FAILURE);
        }

        if((pid == fork()) > 0){
            exit(EXIT_SUCCESS);
        }
        else if(pid < 0){
            fprintf(stderr, "Error encountered during fork\n");
            exit(EXIT_FAILURE);
        }
        else{ //finally deamon process
            for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--){
                close (x);
            }

        }
        
    } 
    return 0;
}
