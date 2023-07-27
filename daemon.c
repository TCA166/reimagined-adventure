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
#include <sys/prctl.h>

#ifndef __linux__
    #error "Compilation on non linux systems not supported"
#endif

//How should the daemon process be called
#define daemonName "directory-monitor"

//Signal reload config
#define SIGRCONFIG SIGUSR1

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

//Struct that ties together event descriptors with configs
struct watchedDirectory{
    int ed;
    dirConfig* config;
};

//Well a hash table h(ed)->config would have been faster, but probably overkill for now. Should we get performance issues though that is something worth looking into

//Initializes an array of watched directories
struct watchedDirectory* initEvents(config* curConfig, int fd);

//Frees everything in wd*, including the alloc'd stuff in configs
void rmEvents(struct watchedDirectory* wd, size_t len, int fd);

struct watchedDirectory* reloadConfig(const char* configPath, struct watchedDirectory* wd, config* curConfig, int inotifyFD);

//Signal handler
void handleSIGRCONFIG(int sig);

//global variables
struct watchedDirectory* wd = NULL;
config* curConfig;
const char* configPath;
int inotifyFD;

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }
    configPath = argv[1];
    if (access(configPath, F_OK) != 0) {
        fprintf(stderr, "Config file couldn't be accessed\n");
        exit(EXIT_FAILURE);
    }
    //Get the initial config, we are doing it here to notify the user of any problems in the config
    curConfig = getConfig(configPath);
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
            prctl(PR_SET_NAME, daemonName);
            argv[0] = daemonName;
            //close all file descriptors
            for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--){
                close (x);
            }
            //Initialise log
            openlog(daemonName, LOG_PID, LOG_DAEMON);
            syslog(LOG_NOTICE, "Activating...");
            //first we need to setup inotify events
            inotifyFD = inotify_init();
            if(inotifyFD < 0){
                syslog(LOG_ERR, "Couldn't initialise inotify");
                exit(EXIT_FAILURE);
            }
            //use initEvents to create the array
            wd = initEvents(curConfig, inotifyFD);
            if(wd == NULL){
                syslog(LOG_ERR, "Couldn't initialize events");
                exit(EXIT_FAILURE);
            }
            //We do this here in order to assure all variables have set values
            signal(SIGRCONFIG, handleSIGRCONFIG); //handle the reload config signal
            while(true){ //Main daemon function
                unsigned char inotifyBuffer[EVENT_BUF_LEN];
                //will wait here until events come
                int length = read(inotifyFD, inotifyBuffer, EVENT_BUF_LEN);
                if(length < 0){
                    syslog(LOG_ERR, "Couldn't read inotify buffer");
                    exit(EXIT_FAILURE);
                }
                int i = 0;
                while(i < length){ //receive all events
                    struct inotify_event *event = (struct inotify_event*)&inotifyBuffer[ i ];
                    if (event->len){
                        if((event->mask & (IN_CREATE | IN_MOVE)) && !(event->mask & IN_ISDIR) && event->wd >= 0){
                            //if we are looking at the correct event type
                            dirConfig* locConfig = NULL; //we need to find the correct config
                            for(int i = 0; i < curConfig->len; i++){
                                if(event->wd == wd[i].ed){
                                    locConfig = wd[i].config;
                                }
                            }
                            //if we didnt find anything then error
                            if(locConfig == NULL){
                                syslog(LOG_ERR, "Invalid event recieved %s", event->name);
                                exit(EXIT_FAILURE);
                            }
                            //Do the main thing
                            if(monitorDirectory(locConfig, false) < 0){
                                syslog(LOG_ERR, "Error encountered in monitorDirectory");
                            }
                        }
                    }
                    i += EVENT_SIZE + event->len;
                }
                if((wd = reloadConfig(configPath, wd, curConfig, inotifyFD)) == NULL){
                    syslog(LOG_NOTICE, "Detected config removal.");
                    break;
                }
            }
            //free everything and exit
            rmEvents(wd, curConfig->len, inotifyFD);
            close(inotifyFD);
            free(curConfig);
            syslog (LOG_NOTICE, "Terminating peacefully...");
            closelog();
            exit(EXIT_SUCCESS);
        }
        
    } 
    return 0;
}

struct watchedDirectory* initEvents(config* curConfig, int fd){
    struct watchedDirectory* wd = calloc(curConfig->len, sizeof(struct watchedDirectory));
    for(int i = 0; i < curConfig->len; i++){
        wd[i].ed = inotify_add_watch(fd, curConfig->partConfigs[i].dirName, IN_CREATE);
        if(wd[i].ed < 0){
            syslog(LOG_ERR, "Couldn't initialise inotify event for %s", curConfig->partConfigs[i].dirName);
            exit(EXIT_FAILURE);
        }
        wd[i].config = curConfig->partConfigs + i;
    }
    return wd;
}

void rmEvents(struct watchedDirectory* wd, size_t len, int fd){
    for(int i = 0; i < len; i++){
        inotify_rm_watch(fd, wd[i].ed);
        close(wd[i].ed);
        freeDirConfig(wd[i].config);
    }
    free(wd);
}

struct watchedDirectory* reloadConfig(const char* configPath, struct watchedDirectory* wd, config* curConfig, int inotifyFD){
    //Update config or crash before going to sleep again
    if (access(configPath, F_OK) != 0) {
        return NULL;
    }
    else{
        //update config and events
        //Possible source of slowdown, look into doing this only N iterations?
        size_t oldSize = curConfig->len;
        struct watchedDirectory* oldWd = wd;
        curConfig = getConfig(configPath);
        wd = initEvents(curConfig, inotifyFD);
        //if a part config in old config isn't in new one then close that watch event
        for(int i = 0; i < oldSize; i++){
            bool found = false;
            for(int n = 0; n < curConfig->len; n++){
                found = strcmp(oldWd[i].config->dirName, wd[i].config->dirName) == 0;
                if(found){
                    break;
                }
            }
            if(!found){
                inotify_rm_watch(inotifyFD, oldWd[i].ed);
                close(oldWd[i].ed);
            }
            freeDirConfig(oldWd[i].config);
        }
        free(oldWd);
        return wd;
    }
}

//I could use goto, and avoid using global variables, but I don't think that's a good idea, though possibly beneficial to performance
void handleSIGRCONFIG(int sig){
    if(sig == SIGRCONFIG && curConfig != NULL && wd != NULL){
        syslog(LOG_NOTICE, "Received SIGRCONFIG signal, reloading config now...");
        if((wd = reloadConfig(configPath, wd, curConfig, inotifyFD)) == NULL){
            syslog(LOG_NOTICE, "Detected config removal.");
            exit(EXIT_SUCCESS);
        }
    }
}
