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

#define logPerror(msg) syslog(LOG_ERR, msg " %s", strerror(errno))

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

//Signal handlers

void handleSIGRCONFIG(int sig);

void handleSIGTERM(int sig);

//global variables
struct watchedDirectory* wd = NULL; //Global array of wd and the associated config
config* curConfig; //The current global config
const char* configPath; //The current global configPath
int inotifyFD; //The current global inotify file descriptor

int main(int argc, char** argv){
    if(argc < 2){
        fprintf(stderr, "Incorrect number of arguments\n");
        exit(EXIT_FAILURE);
    }
    configPath = argv[1];
    if(access(configPath, F_OK) != 0) {
        fprintf(stderr, "Config file couldn't be accessed\n");
        exit(EXIT_FAILURE);
    }
    bool verbose = false;
    bool recursive = false;
    char log = LOG_DAEMON;
    for(int i = 2; i < argc; i++){
        if(strcmp(argv[i], "-v") == 0){
            verbose = true;
        }
        else if(strcmp(argv[i], "-r") == 0){
            recursive = true;
        }
        else if(strcmp(argv[i], "-l") == 0){
            if(argc <= i + 1){
                fprintf(stderr, "No argument following -l");
                exit(EXIT_FAILURE);
            }
            char l = argv[i + 1][0] - '0';
            if(l > 7 || l < 0){
                fprintf(stderr, "Invalid log number");
                exit(EXIT_FAILURE);
            }
            log = (l + 16) << 3; //we emulate the LOG_LOCAL macros
        }
    }
    //Get the initial config, we are doing it here to notify the user of any problems in the config
    curConfig = getConfig(configPath);
    curConfig->recursive |= recursive;
    curConfig->verbose |= verbose;
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
        signal(SIGTERM, handleSIGTERM);
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
            openlog(daemonName, LOG_PID, log);
            syslog(LOG_NOTICE, "Activating...");
            //first we need to setup inotify events
            inotifyFD = inotify_init();
            if(inotifyFD < 0){
                logPerror("Couldn't initialise inotify");
                exit(EXIT_FAILURE);
            }
            //use initEvents to create the array
            wd = initEvents(curConfig, inotifyFD);
            if(wd == NULL){
                logPerror("Couldn't initialize events");
                exit(EXIT_FAILURE);
            }
            //We do this here in order to assure all variables have set values
            signal(SIGRCONFIG, handleSIGRCONFIG); //handle the reload config signal
            //before we enter the main loop of daemon let's do what process does and clean up
            for(int i = 0; i < curConfig->len; i++){
                if(monitorDirectory(curConfig->partConfigs + i, false, curConfig->recursive, curConfig->verbose, curConfig->move) < 0){
                    logPerror("Error encountered in monitorDirectory during initial sweep.");
                    exit(EXIT_FAILURE);
                }
            }
            while(true){ //Main daemon function
                unsigned char inotifyBuffer[EVENT_BUF_LEN];
                //will wait here until events come
                int length = read(inotifyFD, inotifyBuffer, EVENT_BUF_LEN);
                if(length < 0){
                    logPerror("Couldn't read inotify buffer");
                    exit(EXIT_FAILURE);
                }
                int i = 0;
                while(i < length){ //receive all events
                    struct inotify_event *event = (struct inotify_event*)&inotifyBuffer[i];
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
                                syslog(LOG_ERR, "Invalid event recieved %d:%s", event->wd, event->name);
                                exit(EXIT_FAILURE);
                            }
                            //Do the main thing
                            if(monitorDirectory(locConfig, false, curConfig->recursive, curConfig->verbose, curConfig->move) < 0){
                                logPerror("Error encountered in monitorDirectory.");
                                exit(EXIT_FAILURE);
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
            syslog(LOG_NOTICE, "Terminating peacefully...");
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
            return NULL;
        }
        wd[i].config = curConfig->partConfigs + i;
    }
    return wd;
}

void rmEvents(struct watchedDirectory* wd, size_t len, int fd){
    if(wd == NULL){
        return;
    }
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
        config* newConfig = getConfig(configPath);
        wd = initEvents(newConfig, inotifyFD);
        //if a part config in old config isn't in new one then close that watch event
        for(int i = 0; i < oldSize; i++){
            bool found = false;
            for(int n = 0; n < newConfig->len; n++){
                if(wd[i].config != NULL){ //realistically speaking won't happen, but the compiler thinks so
                    found = strcmp(oldWd[i].config->dirName, wd[i].config->dirName) == 0;
                    if(found){
                        break;
                    }
                }
            }
            if(!found){
                inotify_rm_watch(inotifyFD, oldWd[i].ed);
                close(oldWd[i].ed);
            }
            freeDirConfig(oldWd[i].config);
        }
        free(oldWd);
        //we only pass a one level pointer of curConfig, as such we have to transplant the values
        curConfig->len = newConfig->len;
        free(curConfig->partConfigs);
        free(curConfig->move);
        curConfig->partConfigs = newConfig->partConfigs;
        curConfig->move = newConfig->move;
        curConfig->verbose = newConfig->verbose;
        curConfig->recursive = newConfig->recursive;
        free(newConfig);
        return wd;
    }
}

//I could use longjmp, and avoid using global variables, but I don't think that's a good idea, though possibly beneficial to performance
void handleSIGRCONFIG(int sig){
    if(sig == SIGRCONFIG && curConfig != NULL && wd != NULL){
        syslog(LOG_NOTICE, "Received SIGRCONFIG signal, reloading config now...");
        if((wd = reloadConfig(configPath, wd, curConfig, inotifyFD)) == NULL){
            syslog(LOG_NOTICE, "Detected config removal.");
            exit(EXIT_SUCCESS);
        }
    }
}

void handleSIGTERM(int sig){
    if(sig == SIGTERM){
        syslog(LOG_NOTICE, "Received SIGTERM, shutting down as requested.");
        exit(EXIT_SUCCESS);
    }
}
