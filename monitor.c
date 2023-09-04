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

/*!
 @brief Gets the value from a config line
 @param line the line that contains token:value
 @return pointer to the value, or NULL on error 
*/
static char* getConfigValue(char* line);

/*!
 @brief Joins two strings with a char delimiter between
 @param a the first string
 @param b the second string
 @param delimit the char delimiter
 @return the pointer to the newly created string
*/
static char* join(const char* a, const char* b, const char* delimit);

/*!
 @brief Asks the user for confirmation
 @param filename the filename that the request pertains to
 @return true on user accept
*/
static bool userConfirm(const char* filename);

//Cannot point to $. After it is done running and returns non NULL token should point to key
static char* getConfigValue(char* line){
    char* delimit = strchr(line, ':');
    if(delimit != NULL){
        *delimit = '\0';
        char* value = delimit + 1;
        return value;
    }
    return NULL;
}

config* getConfig(char* fileContents){
    config* result = calloc(1, sizeof(config));
    result->len = 0;
    result->partConfigs = NULL;
    result->move = NULL;
    result->recursive = false;
    result->verbose = false;
    dirConfig part;
    part.dirName = NULL;
    part.whitelist = NULL;
    part.whitelistLen = 0;
    part.move = NULL;
    part.recursive = false;
    part.verbose = false;
    char* token = strtok(fileContents, "\n");
    while(token != NULL){
        char* com = strchr(token, '#');
        if(com != NULL){ //ignore comments
            *com = '\0';
        }
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
                            return NULL;
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
                result->move = realpath(value, NULL);
                if(result->move == NULL){
                    return NULL;
                }
            }
            else if(strcmp(token, "recursive") == 0){
                result->recursive = strcmp(value, "true") == 0;
            }
            else if(strcmp(token, "verbose") == 0){
                result->verbose = strcmp(value, "true") == 0;
            }
        }
        else if(token[0] != '#'){ //we are in dir def line
            if(part.dirName != NULL){
                //append the part to the result
                result->partConfigs = realloc(result->partConfigs, (result->len + 1) * sizeof(struct dirConfig));
                result->partConfigs[result->len] = part;
                result->len++;
                //reset the part variable
                part.dirName = NULL;
                part.whitelist = NULL;
                part.whitelistLen = 0;
                part.move = NULL;
                part.recursive = false;
                part.verbose = false;
            }
            part.dirName = realpath(token, NULL);
            if(part.dirName == NULL){
                return NULL;
            }
        }
        token = strtok(NULL, "\n");
    }
    result->partConfigs = realloc(result->partConfigs, (result->len + 1) * sizeof(struct dirConfig));
    result->partConfigs[result->len] = part;
    result->len++;
    return result;
}

config* getConfigFilename(const char* filename){
    //Open the file, if the path is invalid then try to correct it
    FILE* configFile = NULL;
    if(access(filename, F_OK) != 0){
        char* target = realpath(filename, NULL);
        if(target == NULL){
            return NULL;
        }
        configFile = fopen(target, "r");
        free(target);
    }
    else{
        configFile = fopen(filename, "r");
    }
    if(configFile == NULL){
        return NULL;
    }
    if(fseek(configFile, 0, SEEK_END) != 0){
        return NULL;
    }
    size_t fileSize = ftell(configFile);
    if(fseek(configFile, 0, SEEK_SET) != 0){
        return NULL;
    }
    char* fileContents = calloc(fileSize + 1, 1);
    if(fread(fileContents, 1, fileSize, configFile) != fileSize){
        return NULL;
    }
    fclose(configFile);
    config* newConfig = getConfig(fileContents);
    free(fileContents);
    return newConfig;
}

static char* join(const char* a, const char* b, const char* delimit){
    char* result = calloc(strlen(a) + strlen(b) + strlen(delimit) + 1, 1);
    strcpy(result, a);
    strcat(result, delimit);
    strcat(result, b);
    return result;
}

static bool userConfirm(const char* filename){
    while(true){
        printf("Are you sure you want to remove %s?(y/n)", filename);
        char input = '\0';
        if(scanf("%c", &input) == EOF){
            return false;
        }
        if(input == 'y'){
            return true;
        }
        else if(input == 'n'){
            return false;
        }
    }
}

int monitorDirectory(dirConfig* config, bool confirm, bool recursive, bool verbose, char* move){
    const char* dirFilename = config->dirName;
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
                return -2;
            }
            const char* mime = magic_file(magic, (const char*)entityFilename);
            if(mime == NULL){
                return -3;
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
                        return -4;
                    }
                    if(config->verbose || verbose){
                        printf("Moved %s to %s because it wasn't whitelisted\n", entityFilename, newFilename);
                    }
                    free(newFilename);
                }
                else{
                    if(remove(entityFilename) != 0){
                        return -5;
                    }
                    if(config->verbose || verbose){
                        printf("Removed %s because it wasn't whitelisted\n", entity->d_name);
                    }
                }
                deleted++;
            }
            magic_close(magic);
        }
        //for whatever reason .. and . aren't symlinks, but regular directories? IDK why
        else if(entity->d_type == DT_DIR  && (config->recursive || recursive) && !(strcmp(entity->d_name, "..") == 0 || strcmp(entity->d_name, ".") == 0)){ 
            //We are looking at a directory
            dirConfig* subConfig = malloc(sizeof(dirConfig));
            memcpy(subConfig, config, sizeof(dirConfig));
            subConfig->dirName = entityFilename;
            int childResult = monitorDirectory(subConfig, confirm, recursive, verbose, move);
            if(childResult < 0){
                return childResult;
            }
            deleted += childResult;
            free(subConfig);
        }
        free(entityFilename);
    }
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