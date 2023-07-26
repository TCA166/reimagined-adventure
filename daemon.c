#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h> 
#include <magic.h>

#ifndef __unix__
    #error "Compilation on non unix systems not supported"
#endif


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
