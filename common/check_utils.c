/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein - sven.nierlein@consol.de
 *
 * This file is part of mod_gearman.
 *
 *  mod_gearman is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mod_gearman is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mod_gearman.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "config.h"
#include "check_utils.h"
#include "utils.h"
#include "epn_utils.h"
#include "gearman_utils.h"
#include "popenRWE.h"

pid_t current_child_pid = 0;

/* convert number to signal name */
char *nr2signal(int sig) {
    char * signame = NULL;
    switch(sig) {
        case 1:  signame = "SIGHUP";
                 break;
        case 2:  signame = "SIGINT";
                 break;
        case 3:  signame = "SIGQUIT";
                 break;
        case 4:  signame = "SIGILL";
                 break;
        case 5:  signame = "SIGTRAP";
                 break;
        case 6:  signame = "SIGABRT";
                 break;
        case 7:  signame = "SIGBUS";
                 break;
        case 8:  signame = "SIGFPE";
                 break;
        case 9:  signame = "SIGKILL";
                 break;
        case 10: signame = "SIGUSR1";
                 break;
        case 11: signame = "SIGSEGV";
                 break;
        case 12: signame = "SIGUSR2";
                 break;
        case 13: signame = "SIGPIPE";
                 break;
        case 14: signame = "SIGALRM";
                 break;
        case 15: signame = "SIGTERM";
                 break;
        case 16: signame = "SIGURG";
                 break;
        default: signame = malloc(20);
                 snprintf(signame, 20, "signal %d", sig);
                 return signame;
                 break;
    }
    return strdup(signame);
}


/* extract check result */
char *extract_check_result(FILE *fp, int trimmed) {
    int size;
    char buffer[GM_BUFFERSIZE] = "";
    char output[GM_BUFFERSIZE] = "";

    /* get all lines of plugin output - escape newlines */
    strcpy(buffer,"");
    strcpy(output,"");
    size = GM_MAX_OUTPUT;
    while(size > 0 && fgets(buffer,sizeof(buffer)-1,fp)){
        strncat(output, buffer, size);
        size -= strlen(buffer);
    }

    return(gm_escape_newlines(output, trimmed));
}


/* run a check */
int run_check(char *processed_command, char **ret, char **err) {
    char *argv[MAX_CMD_ARGS];
    FILE *fp;
    pid_t pid;
    int pipe_stdout[2], pipe_stderr[2], pipe_rwe[3];
    int retval;
    sigset_t mask;

#ifdef EMBEDDEDPERL
    retval = run_epn_check(processed_command, ret, err);
    if(retval != GM_NO_EPN) {
        return retval;
    }
#endif

    /* check for check execution method (shell or execvp)
     * command line does not have to contain shell meta characters
     * and cmd must begin with a /. Otherwise "BLAH=BLUB cmd" would lead
     * to file not found errors
     */
    if((*processed_command == '/' || *processed_command == '.') && !strpbrk(processed_command,"!$^&*()~[]\\|{};<>?`\"'")) {
        /* use the fast execvp when there are no shell characters */
        gm_log( GM_LOG_TRACE, "using execvp, no shell characters found\n" );

        parse_command_line(processed_command,argv);
        if(!argv[0])
            _exit(STATE_UNKNOWN);

        if(pipe(pipe_stdout)) {
            gm_log( GM_LOG_ERROR, "error creating pipe: %s\n", strerror(errno));
            _exit(STATE_UNKNOWN);
        }
        if(pipe(pipe_stderr)) {
            gm_log( GM_LOG_ERROR, "error creating pipe: %s\n", strerror(errno));
            _exit(STATE_UNKNOWN);
        }
        if((pid=fork())<0){
            gm_log( GM_LOG_ERROR, "fork error\n");
            _exit(STATE_UNKNOWN);
        }
        else if(!pid){
            /* remove all customn signal handler */
            sigfillset(&mask);
            sigprocmask(SIG_UNBLOCK, &mask, NULL);

            /* child process */
            if((dup2(pipe_stdout[1],STDOUT_FILENO)<0)){
                gm_log( GM_LOG_ERROR, "dup2 error\n");
                _exit(STATE_UNKNOWN);
            }
            if((dup2(pipe_stderr[1],STDERR_FILENO)<0)){
                gm_log( GM_LOG_ERROR, "dup2 error\n");
                _exit(STATE_UNKNOWN);
            }
            close(pipe_stdout[1]);
            close(pipe_stderr[1]);
            current_child_pid = getpid();
            execvp(argv[0], argv);
            if(errno == 2)
                _exit(127);
            if(errno == 13)
                _exit(126);
            _exit(STATE_UNKNOWN);
        }

        /* parent */
        /* prepare stdout pipe reading */
        close(pipe_stdout[1]);
        fp=fdopen(pipe_stdout[0],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *ret = extract_check_result(fp, GM_DISABLED);
        fclose(fp);

        /* prepare stderr pipe reading */
        close(pipe_stderr[1]);
        fp=fdopen(pipe_stderr[0],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *err = extract_check_result(fp, GM_ENABLED);
        fclose(fp);

        close(pipe_stdout[0]);
        close(pipe_stderr[0]);
        if(waitpid(pid,&retval,0)!=pid)
            retval=-1;
    }
    else {
        /* use the slower popen when there were shell characters */
        gm_log( GM_LOG_TRACE, "using popen, found shell characters\n" );
        current_child_pid = getpid();
        pid = popenRWE(pipe_rwe, processed_command);

        /* extract check result */
        fp=fdopen(pipe_rwe[1],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *ret = extract_check_result(fp, GM_DISABLED);
        fclose(fp);

        /* extract check stderr */
        fp=fdopen(pipe_rwe[2],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *err = extract_check_result(fp, GM_ENABLED);
        fclose(fp);

        /* close the process */
        retval=pcloseRWE(pid, pipe_rwe);
    }

    return retval;
}


/* execute this command with given timeout */
int execute_safe_command(gm_job_t * exec_job, int fork_exec, char * identifier) {
    int pipe_stdout[2] , pipe_stderr[2];
    int return_code;
    int pclose_result;
    char *plugin_output, *plugin_error;
    char *bufdup;
    char buffer[GM_BUFFERSIZE], buf_error[GM_BUFFERSIZE];
    struct timeval start_time,end_time;
    pid_t pid    = 0;
    buffer[0]    = '\x0';
    buf_error[0] = '\x0';

    gm_log( GM_LOG_TRACE, "execute_safe_command()\n" );

    // mark all filehandles to close on exec
    int x;
    for(x = 0; x<=64; x++)
        fcntl(x, F_SETFD, FD_CLOEXEC);

    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    if(exec_job->start_time.tv_sec == 0) {
        gettimeofday(&start_time,NULL);
        exec_job->start_time = start_time;
    }

    /* fork a child process */
    if(fork_exec == GM_ENABLED) {
        if(pipe(pipe_stdout) != 0)
            perror("pipe stdout");
        if(pipe(pipe_stderr) != 0)
            perror("pipe stderr");

        pid=fork();

        /*fork error */
        if( pid == -1 ) {
            exec_job->output      = strdup("(Error On Fork)");
            exec_job->return_code = 3;
            return(GM_ERROR);
        }
    }

    /* we are in the child process */
    if( fork_exec == GM_DISABLED || pid == 0 ) {

        /* become the process group leader */
        setpgid(0,0);
        pid = getpid();

        if( fork_exec == GM_ENABLED ) {
            close(pipe_stdout[0]);
            close(pipe_stderr[0]);
        }
        signal(SIGALRM, check_alarm_handler);
        alarm(exec_job->timeout);

        /* run the plugin check command */
        pclose_result = run_check(exec_job->command_line, &plugin_output, &plugin_error);
        return_code   = pclose_result;

        if(fork_exec == GM_ENABLED) {
            if(write(pipe_stdout[1], plugin_output, strlen(plugin_output)+1) <= 0)
                perror("write stdout");
            if(write(pipe_stderr[1], plugin_error, strlen(plugin_error)+1) <= 0)
                perror("write");

            if(pclose_result == -1) {
                char error[GM_BUFFERSIZE];
                snprintf(error, sizeof(error), "error on %s: %s", identifier, strerror(errno));
                if(write(pipe_stdout[1], error, strlen(error)+1) <= 0)
                    perror("write");
            }

            return_code = real_exit_code(pclose_result);
            free(plugin_output);
            free(plugin_error);
            _exit(return_code);
        }
        else {
            snprintf( buffer,    sizeof( buffer )-1,    "%s", plugin_output );
            snprintf( buf_error, sizeof( buf_error )-1, "%s", plugin_error  );
            free(plugin_output);
            free(plugin_error);
        }
    }

    /* we are the parent */
    if( fork_exec == GM_DISABLED || pid > 0 ){

        if( fork_exec == GM_ENABLED) {
            gm_log( GM_LOG_TRACE, "started check with pid: %d\n", pid);

            close(pipe_stdout[1]);
            close(pipe_stderr[1]);

            waitpid(pid, &return_code, 0);
            gm_log( GM_LOG_TRACE, "finished check from pid: %d with status: %d\n", pid, return_code);
            /* get all lines of plugin output */
            if(read(pipe_stdout[0], buffer, sizeof(buffer)-1) < 0)
                perror("read");
            if(read(pipe_stderr[0], buf_error, sizeof(buf_error)-1) < 0)
                perror("read");
        }
        return_code = real_exit_code(return_code);

        /* file not executable? */
        if(return_code == 126) {
            return_code = STATE_CRITICAL;
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. (worker: %s)", identifier);
        }
        /* file not found errors? */
        else if(return_code == 127) {
            return_code = STATE_CRITICAL;
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of 127 is out of bounds. Make sure the plugin you're trying to run actually exists. (worker: %s)", identifier);
        }
        /* signaled */
        else if(return_code >= 128 && return_code < 144) {
            char * signame = nr2signal((int)(return_code-128));
            bufdup = strdup(buffer);
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of %d is out of bounds. Plugin exited by signal %s. (worker: %s)\\n%s", (int)(return_code), signame, identifier, bufdup);
            return_code = STATE_CRITICAL;
            free(bufdup);
            free(signame);
        }
        /* other error codes > 3 */
        else if(return_code > 3) {
            gm_log( GM_LOG_DEBUG, "check exited with exit code > 3. Exit: %d\n", (int)(return_code));
            gm_log( GM_LOG_DEBUG, "stdout: %s\n", buffer);
            bufdup = strdup(buffer);
            snprintf( buffer, sizeof( buffer )-1, "CRITICAL: Return code of %d is out of bounds. (worker: %s)\\n%s", (int)(return_code), identifier, bufdup);
            free(bufdup);
            if(return_code != 25 && mod_gm_opt->workaround_rc_25 == GM_DISABLED) {
                return_code = STATE_CRITICAL;
            }
        }

        exec_job->output      = strdup(buffer);
        exec_job->error       = strdup(buf_error);
        exec_job->return_code = return_code;
        if( fork_exec == GM_ENABLED) {
            close(pipe_stdout[0]);
            close(pipe_stderr[0]);
        }
    }
    alarm(0);
    current_child_pid = 0;
    pid               = 0;

    /* record check result info */
    gettimeofday(&end_time, NULL);
    exec_job->finish_time = end_time;

    /* did we have a timeout? */
    if(exec_job->timeout < ((int)end_time.tv_sec - (int)exec_job->start_time.tv_sec)) {
        exec_job->return_code   = 2;
        exec_job->early_timeout = 1;
        if ( !strcmp( exec_job->type, "service" ) )
            snprintf( buffer, sizeof( buffer ) -1, "(Service Check Timed Out On Worker: %s)", identifier);
        if ( !strcmp( exec_job->type, "host" ) )
            snprintf( buffer, sizeof( buffer ) -1, "(Host Check Timed Out On Worker: %s)", identifier);
        free(exec_job->output);
        exec_job->output = strdup( buffer );
    }

    return(GM_OK);
}


/* called when check runs into timeout */
void check_alarm_handler(int sig) {
    pid_t pid;

    gm_log( GM_LOG_TRACE, "check_alarm_handler(%i)\n", sig );
    pid = getpid();
    if(current_job != NULL && mod_gm_opt->fork_on_exec == GM_DISABLED) {
        /* create a useful log message*/
        if ( !strcmp( current_job->type, "service" ) ) {
            gm_log( GM_LOG_INFO, "timeout (%is) hit for servicecheck: %s - %s\n", current_job->timeout, current_job->host_name, current_job->service_description);
        }
        else if ( !strcmp( current_job->type, "host" ) ) {
            gm_log( GM_LOG_INFO, "timeout (%is) hit for hostcheck: %s\n", current_job->timeout, current_job->host_name);
        }
        else if ( !strcmp( current_job->type, "eventhandler" ) ) {
            gm_log( GM_LOG_INFO, "timeout (%is) hit for eventhandler: %s\n", current_job->timeout, current_job->command_line);
        }
        send_timeout_result(current_job);
        gearman_job_send_complete(current_gearman_job, NULL, 0);
    }

    signal(SIGTERM, SIG_IGN);
    gm_log( GM_LOG_TRACE, "send SIGTERM to %d\n", pid);
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
    signal(SIGTERM, SIG_DFL);
    sleep(1);

    signal(SIGINT, SIG_IGN);
    gm_log( GM_LOG_TRACE, "send SIGINT to %d\n", pid);
    kill(-pid, SIGINT);
    kill(pid, SIGINT);
    signal(SIGINT, SIG_DFL);
    sleep(1);

    // skip sigkill in test mode
    if(getenv("MODGEARMANTEST") == NULL) {
        gm_log( GM_LOG_TRACE, "send SIGKILL to %d\n", pid);
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
    }

    return;
}

/* send kill to all forked processes */
void kill_child_checks(void) {
    int retval;
    pid_t pid;

    signal(SIGINT, SIG_IGN);
    pid = getpid();
    if(current_child_pid > 0 && current_child_pid != pid) {
        gm_log( GM_LOG_TRACE, "kill_child_checks(): send SIGINT to %d\n", current_child_pid);
        kill(-current_child_pid, SIGINT);
        kill(current_child_pid, SIGINT);
        sleep(1);
        if(waitpid(current_child_pid,&retval,WNOHANG)!=0) {
            signal(SIGINT, SIG_DFL);
            return;
        }
        if(pid_alive(current_child_pid)) {
            gm_log( GM_LOG_TRACE, "kill_child_checks(): send SIGKILL to %d\n", current_child_pid);
            kill(current_child_pid, SIGKILL);
        }
    }
    gm_log( GM_LOG_TRACE, "send SIGINT to %d\n", pid);
    kill(0, SIGINT);
    signal(SIGINT, SIG_DFL);
    return;
}


void send_timeout_result(gm_job_t * exec_job) {
    struct timeval end_time;
    char buffer[GM_BUFFERSIZE];
    buffer[0] = '\x0';

    gm_log( GM_LOG_TRACE, "send_timeout_result()\n");

    gettimeofday(&end_time, NULL);
    exec_job->finish_time = end_time;

    exec_job->return_code   = mod_gm_opt->timeout_return;
    exec_job->early_timeout = 1;
    if ( !strcmp( exec_job->type, "service" ) )
        snprintf( buffer, sizeof( buffer ) -1, "(Service Check Timed Out On Worker: %s)\n", mod_gm_opt->identifier);
    if ( !strcmp( exec_job->type, "host" ) )
        snprintf( buffer, sizeof( buffer ) -1, "(Host Check Timed Out On Worker: %s)\n", mod_gm_opt->identifier);
    free(exec_job->output);
    exec_job->output = strdup( buffer );

    send_result_back(exec_job);

    return;
}


/* send failed result */
void send_failed_result(gm_job_t * exec_job, int sig) {
    struct timeval end_time;
    char buffer[GM_BUFFERSIZE];
    char * signame;
    buffer[0] = '\x0';

    gm_log( GM_LOG_TRACE, "send_failed_result()\n");

    gettimeofday(&end_time, NULL);
    exec_job->finish_time = end_time;
    exec_job->return_code = STATE_CRITICAL;

    signame = nr2signal(sig);
    snprintf( buffer, sizeof( buffer )-1, "(Return code of %d is out of bounds. Worker exited by signal %s on worker: %s)", sig, signame, mod_gm_opt->identifier);
    free(exec_job->output);
    exec_job->output = strdup( buffer );
    free(signame);

    send_result_back(exec_job);

    return;
}


/* convert a command line to an array of arguments, suitable for exec* functions */
int parse_command_line(char *cmd, char *argv[MAX_CMD_ARGS]) {
    unsigned int argc=0;
    char *parsed_cmd;

    /* Skip initial white-space characters. */
    for(parsed_cmd=cmd;isspace(*cmd);++cmd)
        ;

    /* Parse command line. */
    while(*cmd&&(argc<MAX_CMD_ARGS-1)){
        argv[argc++]=parsed_cmd;
        switch(*cmd){
        case '\'':
            while((*cmd)&&(*cmd!='\''))
                *(parsed_cmd++)=*(cmd++);
            if(*cmd)
                ++cmd;
            break;
        case '"':
            while((*cmd)&&(*cmd!='"')){
                if((*cmd=='\\')&&cmd[1]&&strchr("\"\\\n",cmd[1]))
                    ++cmd;
                *(parsed_cmd++)=*(cmd++);
                }
            if(*cmd)
                ++cmd;
            break;
        default:
            while((*cmd)&&!isspace(*cmd)){
                if((*cmd=='\\')&&cmd[1])
                    ++cmd;
                *(parsed_cmd++)=*(cmd++);
                }
            }
        while(isspace(*cmd))
            ++cmd;
        *(parsed_cmd++)='\0';
        }
    argv[argc]=NULL;

    return GM_OK;
}
