#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "monitor.h"
#include <syslog.h>
#include <errno.h>
#include <signal.h>
//prctl is non portable unfortunately
#ifdef __linux__ 
#include <sys/prctl.h>
#endif

#ifndef __unix__
    #error "Compilation on non unix systems not supported"
#endif

#define hour 3600

#define daemonName "directory-monitor"

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
            #ifdef __linux__
            prctl(PR_SET_NAME, daemonName);
            #endif
            argv[0] = daemonName;
            for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--){
                close (x);
            }
            openlog(daemonName, LOG_PID, LOG_DAEMON);
            syslog(LOG_NOTICE, "Directory Monitor Daemon activating...");
            while(true){
                if (access(configPath, F_OK) != 0) {
                    syslog (LOG_ERR, "Couldn't find config file, terminating...");
                    exit(EXIT_FAILURE);
                }
                config curConfig = getConfig(configPath);
                for(int i = 0; i < curConfig.len; i++){
                    if(monitorDirectory(curConfig.partConfigs[i], false, false, false) < 0){
                        syslog(LOG_ERR, "Error encountered in monitorDirectory. errno=%d", errno);
                        exit(EXIT_FAILURE);
                    }
                    freeDirConfig(curConfig.partConfigs[i]);
                }
                free(curConfig.partConfigs);
                unsigned int sleepDiff = sleep(hour);
                if(sleepDiff > 0){
                    break;
                }
            }
            syslog (LOG_NOTICE, "Directory Monitor Daemon terminating peacefully...");
            closelog();
            exit(EXIT_SUCCESS);
        }
        
    } 
    return 0;
}
