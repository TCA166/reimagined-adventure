#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h> 
#include <magic.h>
#include <stdbool.h>
#include <errno.h>

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
    char* fileContents = calloc(fileSize + 1, 1);
    if(fread(fileContents, 1, fileSize, configFile) != fileSize){
        exit(EXIT_FAILURE);
    }
    fclose(configFile);
    config result;
    result.len = 0;
    result.partConfigs = NULL;
    dirConfig part;
    part.dirName = NULL;
    part.whitelist = NULL;
    part.whitelistLen = 0;
    char* token = strtok(fileContents, "\n");
    while(token != NULL){
        if(token[0] == '\t' || token[0] == ' '){ //we are in whitelist line
            token++;
            while(*token == ' '){
                token++;
            }
            part.whitelist = realloc(part.whitelist, (part.whitelistLen + 1) * sizeof(char*));
            part.whitelist[part.whitelistLen] = malloc(strlen(token) + 1);
            strcpy(part.whitelist[part.whitelistLen], token);
            part.whitelistLen++;
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

int monitorDirectory(dirConfig config, bool recursive, bool verbose, bool confirm){
    const char* dirFilename = realpath(config.dirName, NULL);
    if(dirFilename == NULL){
        return -1;
    }
    else{
        errno = 0;
    }
    DIR* directory = opendir(dirFilename);
    if(directory == NULL){
        return -1;
    }
    int deleted = 0;
    struct dirent* entity; //Our currently analyzed entity
    while((entity = readdir(directory)) != NULL){
        char* entityFilename = calloc(strlen(dirFilename) + strlen(entity->d_name) + 2, 1);
        strcpy(entityFilename, dirFilename);
        strcat(entityFilename, "/");
        strcat(entityFilename, entity->d_name);
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
            for(int i = 0; i < config.whitelistLen; i++){
                if(strcmp(mime, config.whitelist[i]) == 0 || strcmp(entityFilename, config.whitelist[i]) == 0 || strcmp(entity->d_name, config.whitelist[i]) == 0){
                    found = true;
                    if(verbose){
                        printf("Skipping %s because it is whitelisted\n", entity->d_name);
                    }
                    break;
                }
            }
            if(!found && (!confirm || userConfirm(entityFilename))){
                if(remove(entityFilename) != 0){
                    return -1;
                }
                if(verbose){
                    printf("Removed %s because it wasn't whitelisted\n", entity->d_name);
                }
                deleted++;
            }
            magic_close(magic);
        }
        else if(entity->d_type == DT_DIR && recursive){ //We are looking at a directory
            dirConfig subConfig = config;
            subConfig.dirName = entityFilename;
            if(monitorDirectory(subConfig, recursive, verbose, confirm) < 0){
                return -1;
            }
        }
        free(entityFilename);
    }
    free((char*)dirFilename);
    closedir(directory);
    return deleted;
}

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
    config mainConfig = getConfig(configPath);
    printf("Config loaded\n");  
    for(int i = 0; i < mainConfig.len; i++){
        if(monitorDirectory(mainConfig.partConfigs[i], recursive, verbose, confirmMode) < 0){
            perror("Error encountered in monitorDirectory");
        }
        free(mainConfig.partConfigs[i].dirName);
        for(int n = 0; n < mainConfig.partConfigs[i].whitelistLen; n++){
            free(mainConfig.partConfigs[i].whitelist[n]);
        }
        free(mainConfig.partConfigs[i].whitelist);
    } 
    free(mainConfig.partConfigs);
    return 0;
}