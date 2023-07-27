#define _DEFAULT_SOURCE
#include "monitor.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h> 
#include <magic.h>
#include <errno.h>

//If compiling with -DDAEMON monitor will be compiled for printing to syslog
#ifdef DAEMON
    #include "printOverride.h"
#endif

//Cannot point to $. After it is done running and returns non NULL token should point to key
char* getConfigValue(char* token){
    char* delimit = strchr(token, ':');
    if(delimit != NULL){
        *delimit = '\0';
        char* value = delimit + 1;
        return value;
    }
    return NULL;
}

config* getConfig(const char* filename){
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
    char* fileContents = calloc(fileSize + 1, 1);
    if(fread(fileContents, 1, fileSize, configFile) != fileSize){
        exit(EXIT_FAILURE);
    }
    fclose(configFile);
    config result;
    result.len = 0;
    result.partConfigs = NULL;
    result.move = NULL;
    result.recursive = false;
    result.verbose = false;
    dirConfig part;
    part.dirName = NULL;
    part.whitelist = NULL;
    part.whitelistLen = 0;
    part.move = NULL;
    part.recursive = false;
    part.verbose = false;
    char* token = strtok(fileContents, "\n");
    while(token != NULL){
        if(token[0] == '\t' || token[0] == ' '){ //we are in local line
            token++;
            //and now we skip forward all tabs and spaces
            while(*token == ' ' || *token == '\t'){
                token++;
            }
            if(*token == '$'){ //we are in a local variable definition line
                token++;
                char* value = getConfigValue(token);
                if(value != NULL){
                    if(strcmp(token, "move") == 0){
                        part.move = realpath(value, NULL);
                        if(part.move == NULL){
                            perror("Invalid path provided to move");
                            exit(EXIT_FAILURE);
                        }
                    }
                    else if(strcmp(token, "recursive") == 0){
                        part.recursive = strcmp(value, "true") == 0;
                    }
                    else if(strcmp(token, "verbose") == 0){
                        part.verbose = strcmp(value, "true") == 0;
                    }
                }
            }
            else{ //we are in a whitelist line
                part.whitelist = realloc(part.whitelist, (part.whitelistLen + 1) * sizeof(char*));
                part.whitelist[part.whitelistLen] = malloc(strlen(token) + 1);
                strcpy(part.whitelist[part.whitelistLen], token);
                part.whitelistLen++;
            }
        }
        else if(token[0] == '$'){
            token++;
            char* value = getConfigValue(token);
            if(strcmp(token, "move") == 0){
                result.move = realpath(value, NULL);
                if(result.move == NULL){
                    perror("Invalid path provided to move");
                    exit(EXIT_FAILURE);
                }
            }
            else if(strcmp(token, "recursive") == 0){
                result.recursive = strcmp(value, "true") == 0;
            }
            else if(strcmp(token, "verbose") == 0){
                result.verbose = strcmp(value, "true") == 0;
            }
        }
        else{ //we are in dir def line
            if(part.dirName != NULL){
                //append the part to the result
                result.partConfigs = realloc(result.partConfigs, (result.len + 1) * sizeof(struct dirConfig));
                result.partConfigs[result.len] = part;
                result.len++;
                //reset the part variable
                part.dirName = NULL;
                part.whitelist = NULL;
                part.whitelistLen = 0;
                part.move = NULL;
                part.recursive = false;
                part.verbose = false;
            }
            part.dirName = malloc(strlen(token) + 1);
            strcpy(part.dirName, token);
        }
        token = strtok(NULL, "\n");
    }
    free(fileContents);
    result.partConfigs = realloc(result.partConfigs, (result.len + 1) * sizeof(struct dirConfig));
    result.partConfigs[result.len] = part;
    result.len++;
    config* resultPtr = malloc(sizeof(config));
    *resultPtr = result;
    return resultPtr;
}

char* join(const char* a, const char* b, const char* delimit){
    char* result = calloc(strlen(a) + strlen(b) + strlen(delimit) + 1, 1);
    strcpy(result, a);
    strcat(result, delimit);
    strcat(result, b);
    return result;
}

bool userConfirm(const char* filename){
    while(true){
        printf("Are you sure you want to remove %s?(y/n)", filename);
        char input = '\0';
        scanf("%c", &input);
        if(input == 'y'){
            return true;
        }
        else if(input == 'n'){
            return false;
        }
    }
}

int monitorDirectory(dirConfig* config, bool confirm, bool recursive, bool verbose, char* move){
    const char* dirFilename = realpath(config->dirName, NULL);
    if(dirFilename == NULL){
        return -1;
    }
    else{
        errno = 0;
    }
    DIR* directory = opendir(dirFilename);
    if(directory == NULL){
        //errno is set by opendir
        return -1;
    }
    int deleted = 0;
    struct dirent* entity; //Our currently analyzed entity
    while((entity = readdir(directory)) != NULL){
        char* entityFilename = join(dirFilename, entity->d_name, "/");
        if(entity->d_type == DT_REG){ //We are looking at a file
            magic_t magic = magic_open(MAGIC_MIME_TYPE);
            if(magic_load(magic, NULL) < 0){
                return -1;
            }
            const char* mime = magic_file(magic, (const char*)entityFilename);
            if(mime == NULL){
                return -1;
            }
            bool found = false;
            for(int i = 0; i < config->whitelistLen; i++){
                if(strcmp(mime, config->whitelist[i]) == 0 || strcmp(entityFilename, config->whitelist[i]) == 0 || strcmp(entity->d_name, config->whitelist[i]) == 0){
                    found = true;
                    if(config->verbose || verbose){
                        printf("Skipping %s because it is whitelisted\n", entity->d_name);
                    }
                    break;
                }
            }
            if(!found && (!confirm || userConfirm(entityFilename))){
                if(config->move != NULL || move != NULL){
                    char* movePath = NULL;
                    if(config->move != NULL){
                        movePath = config->move;
                    }
                    else{
                        movePath = move;
                    }
                    char* newFilename = join(movePath, entity->d_name, "/");
                    if(rename(entityFilename, newFilename) < 0){
                        return -1;
                    }
                    if(config->verbose || verbose){
                        printf("Moved %s to %s because it wasn't whitelisted\n", entityFilename, newFilename);
                    }
                    free(newFilename);
                }
                else{
                    if(remove(entityFilename) != 0){
                        return -1;
                    }
                    if(config->verbose || verbose){
                        printf("Removed %s because it wasn't whitelisted\n", entity->d_name);
                    }
                }
                deleted++;
            }
            magic_close(magic);
        }
        else if(entity->d_type == DT_DIR && (config->recursive || recursive)){ //We are looking at a directory
            dirConfig* subConfig = malloc(sizeof(dirConfig));
            memcpy(subConfig, config, sizeof(dirConfig));
            subConfig->dirName = entityFilename;
            if(monitorDirectory(subConfig, confirm, recursive, verbose, move) < 0){
                return -1;
            }
            free(subConfig);
        }
        free(entityFilename);
    }
    free((char*)dirFilename);
    closedir(directory);
    return deleted;
}

void freeDirConfig(dirConfig* config){
    free(config->dirName);
    for(int n = 0; n < config->whitelistLen; n++){
        free(config->whitelist[n]);
    }
    free(config->whitelist);
}