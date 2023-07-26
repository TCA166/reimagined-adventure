#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "monitor.h"
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/inotify.h>
//prctl is non portable unfortunately
#ifdef __linux__ 
#include <sys/prctl.h>
#endif

#ifndef __unix__
    #error "Compilation on non unix systems not supported"
#endif

#define daemonName "directory-monitor"

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

//Struct that ties together event descriptors with configs
struct watchedDirectory{
    int ed;
    dirConfig config;
};

//Well a hash table h(ed)->would have been faster, but probably overkill for now. Should we get performance issues though that is something worth looking into

//Initializes an array of watched directories
struct watchedDirectory* initEvents(config curConfig, int fd);

//Frees everything in wd*, including the alloc'd stuff in configs
void rmEvents(struct watchedDirectory* wd, size_t len, int fd);

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
        if(setsid() < 0){
            fprintf(stderr, "Error encountered during setsid\n");
            exit(EXIT_FAILURE);
        }
        signal(SIGCHLD, SIG_IGN);
        signal(SIGHUP, SIG_IGN);
        if((pid == fork()) > 0){
            exit(EXIT_SUCCESS);
        }
        else if(pid < 0){
            fprintf(stderr, "Error encountered during fork\n");
            exit(EXIT_FAILURE);
        }
        else{ //finally deamon process
            //set the daemon name
            #ifdef __linux__
            prctl(PR_SET_NAME, daemonName);
            #endif
            argv[0] = daemonName;
            //close all file descriptors
            for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--){
                close (x);
            }
            //Initialise log
            openlog(daemonName, LOG_PID, LOG_DAEMON);
            syslog(LOG_NOTICE, "Activating...");
            //Get the initial config
            config curConfig = getConfig(configPath);
            //first we need to setup inotify events
            int fd = inotify_init();
            if(fd < 0){
                syslog(LOG_ERR, "Couldn't initialise inotify");
                exit(EXIT_FAILURE);
            }
            //use initEvents to create the array
            struct watchedDirectory* wd = initEvents(curConfig, fd);
            while(true){ //Main daemon function
                unsigned char inotifyBuffer[EVENT_BUF_LEN];
                //will wait here until events come
                int length = read(fd, inotifyBuffer, EVENT_BUF_LEN);
                if(length < 0){
                    syslog(LOG_ERR, "Couldn't read inotify buffer");
                    exit(EXIT_FAILURE);
                }
                int i = 0;
                while(i < length){ //receive all events
                    struct inotify_event *event = (struct inotify_event*)&inotifyBuffer[ i ];
                    if (event->len){
                        if((event->mask & IN_CREATE) && !(event->mask & IN_ISDIR)){
                            //if we are looking at the correct event type
                            dirConfig* locConfig = NULL; //we need to find the correct config
                            for(int i = 0; i < curConfig.len; i++){
                                if(event->wd == wd[i].ed){
                                    locConfig = &wd[i].config;
                                }
                            }
                            //if we didnt find anything then error
                            if(locConfig == NULL){
                                syslog(LOG_ERR, "Invalid event recieved %s", event->name);
                                exit(EXIT_FAILURE);
                            }
                            //Do the main thing
                            if(monitorDirectory(*locConfig, false, false, false) < 0){
                                syslog(LOG_ERR, "Error encountered in monitorDirectory");
                            }
                        }
                    }
                    i += EVENT_SIZE + event->len;
                }
                //Update config or crash before going to sleep again
                if (access(configPath, F_OK) != 0) {
                    syslog(LOG_NOTICE, "Detected config removal.");
                    break;
                }
                else{
                    //update config and events
                    //Possible source of slowdown, look into doing this only N iterations?
                    rmEvents(wd, curConfig.len, fd);
                    free(curConfig.partConfigs);
                    curConfig = getConfig(configPath);
                    wd = initEvents(curConfig, fd);
                }
            }
            //free everything and exit
            rmEvents(wd, curConfig.len, fd);
            close(fd);
            syslog (LOG_NOTICE, "Terminating peacefully...");
            closelog();
            exit(EXIT_SUCCESS);
        }
        
    } 
    return 0;
}

struct watchedDirectory* initEvents(config curConfig, int fd){
    struct watchedDirectory* wd = calloc(curConfig.len, sizeof(struct watchedDirectory));
    for(int i = 0; i < curConfig.len; i++){
        wd[i].ed = inotify_add_watch(fd, curConfig.partConfigs[i].dirName, IN_CREATE);
        if(wd[i].ed < 0){
            syslog(LOG_ERR, "Couldn't initialise inotify event for %s", curConfig.partConfigs[i].dirName);
            exit(EXIT_FAILURE);
        }
        wd[i].config = curConfig.partConfigs[i];
    }
    return wd;
}

void rmEvents(struct watchedDirectory* wd, size_t len, int fd){
    for(int i = 0; i < len; i++){
        inotify_rm_watch(fd, wd[i].ed);
        freeDirConfig(wd[i].config);
    }
    free(wd);
}
