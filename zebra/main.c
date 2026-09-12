#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "pty.h"
#include "emulator.h"

int runParent(Pty *pty) {
    close(pty->slave);
    int err = runGUI(pty->master);
    if (err) return 1;
    return 0;
}

int runChild(Pty *pty){
    // Get user's default shell through pw->pw_shell instead of hardcoding bash		
    uid_t uid = getuid();                 
    struct passwd *pw = getpwuid(uid);
    
    if (pw == NULL) {
        perror("getpwuid");
        return 1;
    }

    close(pty->master);
    // with setsid()
    // the process is the leader of the new session
    // we can use ioctl() with TIOCSCTTY
    if (!setsid()) {
        perror("setsid");
        return 1;
    }
    // pty->slave becomes the control terminal of the process
    int set = ioctl(pty->slave, TIOCSCTTY, 0);
    if (set < 0){
        perror("ioctl");
        return 1;
    }
    // redirection
    dup2(pty->slave, 0);
    dup2(pty->slave, 1);
    dup2(pty->slave, 2);
    execvp(pw->pw_shell, (char *[]){pw->pw_shell, NULL});
    return 0;
}

int main (){
    Pty pty;
    int err = get_master(&pty);
    if (err) return 1;
    err = get_slave(&pty);
    if (err) return 1;

    // with fork()
    // the child process is not the pgroup leader
    // we can use setsid()
    pid_t pid = fork();
    if (pid == 0) err = runChild(&pty);
    else if (pid > 0) err = runParent(&pty);
    else return 1;
    if (err) return 1;

    clean_pty(&pty);
    return 0;
}
