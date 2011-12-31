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

/* include header */
#include "config.h"
#include "worker.h"
#include "utils.h"
#include "worker_client.h"

int current_number_of_workers                = 0;
volatile sig_atomic_t current_number_of_jobs = 0;  /* must be signal safe */

int     orig_argc;
char ** orig_argv;
int     last_time_increased;
volatile sig_atomic_t shmid;
int   * shm;
#ifdef EMBEDDEDPERL
extern char *p1_file;
char **start_env;
#endif


/* work starts here */
#ifdef EMBEDDEDPERL
int main (int argc, char **argv, char **env) {
    start_env=env;
    struct stat stat_buf;
#else
int main (int argc, char **argv) {
#endif
    int sid, x;
    last_time_increased = 0;

    /* store the original command line for later reloads */
    store_original_comandline(argc, argv);

    /*
     * allocate options structure
     * and parse command line
     */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    if(parse_arguments(argc, argv) != GM_OK) {
        exit( EXIT_FAILURE );
    }

#ifdef EMBEDDEDPERL
    /* make sure the P1 file exists... */
    if(p1_file==NULL){
        gm_log(GM_LOG_ERROR,"Error: p1.pl file required for embedded Perl interpreter is not set!\n");
        exit( EXIT_FAILURE );
    }
    if(stat(p1_file,&stat_buf)!=0){
        gm_log(GM_LOG_ERROR,"Error: p1.pl file required for embedded Perl interpreter is missing!\n");
        perror("stat");
        exit( EXIT_FAILURE );
    }
#endif

    /* fork into daemon mode? */
    if(mod_gm_opt->daemon_mode == GM_ENABLED) {
        pid_t pid = fork();
        /* an error occurred while trying to fork */
        if(pid == -1) {
            perror("fork");
            exit( EXIT_FAILURE );
        }
        /* we are the child process */
        else if(pid == 0) {
            gm_log( GM_LOG_INFO, "mod_gearman worker daemon started with pid %d\n", getpid());

            /* Create a new SID for the child process */
            sid = setsid();
            if ( sid < 0 ) {
                mod_gm_free_opt(mod_gm_opt);
                exit( EXIT_FAILURE );
            }

            /* Close out the standard file descriptors */
            if(mod_gm_opt->debug_level <= 1) {
                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
            }
        }
        /* we are the parent. So forking into daemon mode worked */
        else {
            mod_gm_free_opt(mod_gm_opt);
            exit( EXIT_SUCCESS );
        }
    } else {
        gm_log( GM_LOG_INFO, "mod_gearman worker started with pid %d\n", getpid());
    }

    /* print some version information */
    gm_log( GM_LOG_DEBUG, "Version %s\n", GM_VERSION );
    gm_log( GM_LOG_DEBUG, "running on libgearman %s\n", gearman_version() );


    /* set signal handlers for a clean exit */
    signal(SIGINT, clean_exit);
    signal(SIGTERM,clean_exit);
    signal(SIGHUP, reload_config);
    signal(SIGPIPE, SIG_IGN);


    /* check and write pid file */
    if(write_pid_file() != GM_OK) {
        exit(EXIT_FAILURE);
    }

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    gm_log( GM_LOG_DEBUG, "main process started\n");

    /* start a single non forked standalone worker */
    if(mod_gm_opt->debug_level >= 10) {
        gm_log( GM_LOG_TRACE, "starting standalone worker\n");
#ifdef EMBEDDEDPERL
        worker_client(GM_WORKER_STANDALONE, 1, shmid, start_env);
#else
        worker_client(GM_WORKER_STANDALONE, 1, shmid);
#endif
        exit(EXIT_SUCCESS);
    }

    /* setup shared memory */
    setup_child_communicator();

    /* start status worker */
    make_new_child(GM_WORKER_STATUS);

    /* setup childs */
    for(x=0; x < mod_gm_opt->min_worker; x++) {
        make_new_child(GM_WORKER_MULTI);
    }

    /* maintain worker population */
    monitor_loop();

    clean_exit(15);
    exit( EXIT_SUCCESS );
}


/* main loop for checking worker */
void monitor_loop() {
    int status;

    /* maintain the population */
    while (1) {
        /* check number of workers every second */
        sleep(GM_DEFAULT_WORKER_LOOP_SLEEP);

        /* collect finished workers */
        while(waitpid(-1, &status, WNOHANG) > 0)
            gm_log( GM_LOG_TRACE, "waitpid() worker exited with: %d\n", status);

        check_worker_population();
    }
    return;
}


/* count current worker and jobs */
void count_current_worker(int restart) {
    int x;

    gm_log( GM_LOG_TRACE3, "count_current_worker()\n");
    gm_log( GM_LOG_TRACE3, "done jobs:     shm[0] = %d\n", shm[0]);

    /* shm states:
     *   0 -> undefined
     *  -1 -> free
     * <-1 -> used but idle
     * > 1 -> used and working
     */

    /* check if status worker died */
    if( shm[3] != -1 && pid_alive(shm[3]) == FALSE ) {
        gm_log( GM_LOG_TRACE, "removed stale status worker, old pid: %d\n", shm[3] );
        shm[3] = -1;
    }
    gm_log( GM_LOG_TRACE3, "status worker: shm[3] = %d\n", shm[3]);

    /* check all known worker */
    current_number_of_workers = 0;
    current_number_of_jobs    = 0;
    for(x=4; x < mod_gm_opt->max_worker+4; x++) {
        /* verify worker is alive */
        gm_log( GM_LOG_TRACE3, "worker slot:   shm[%d] = %d\n", x, shm[x]);
        if( shm[x] != -1 && pid_alive(shm[x]) == FALSE ) {
            gm_log( GM_LOG_TRACE, "removed stale worker %d, old pid: %d\n", x, shm[x]);
            shm[x] = -1;
            /* immediately start new worker, otherwise the fork rate cannot be guaranteed */
            if(restart == GM_ENABLED) {
                make_new_child(GM_WORKER_MULTI);
                current_number_of_workers++;
            }
        }
        if(shm[x] != -1) {
            current_number_of_workers++;
        }
        if(shm[x] > 0) {
            current_number_of_jobs++;
        }
    }

    shm[1] = current_number_of_workers; /* total worker   */
    shm[2] = current_number_of_jobs;    /* running worker */

    gm_log( GM_LOG_TRACE3, "worker: %d  -  running: %d\n", current_number_of_workers, current_number_of_jobs);

    return;
}

/* start new worker if needed */
void check_worker_population() {
    int x, now, target_number_of_workers;

    gm_log( GM_LOG_TRACE3, "check_worker_population()\n");

    /* set current worker number */
    count_current_worker(GM_ENABLED);

    /* check if status worker died */
    if( shm[3] == -1 ) {
        make_new_child(GM_WORKER_STATUS);
    }

    /* keep up minimum population */
    for (x = current_number_of_workers; x < mod_gm_opt->min_worker; x++) {
        make_new_child(GM_WORKER_MULTI);
        current_number_of_workers++;
    }

    /* check every second */
    now = (int)time(NULL);
    if(last_time_increased >= now)
        return;

    target_number_of_workers = adjust_number_of_worker(mod_gm_opt->min_worker, mod_gm_opt->max_worker, current_number_of_workers, current_number_of_jobs);
    for (x = current_number_of_workers; x < target_number_of_workers; x++) {
        last_time_increased = now;
        /* top up the worker pool */
        make_new_child(GM_WORKER_MULTI);
    }
    return;
}


/* start up new worker */
int make_new_child(int mode) {
    pid_t pid = 0;
    int next_shm_index;

    gm_log( GM_LOG_TRACE, "make_new_child(%d)\n", mode);

    if(mode == GM_WORKER_STATUS) {
        gm_log( GM_LOG_TRACE, "forking status worker\n");
        next_shm_index = 3;
    } else {
        gm_log( GM_LOG_TRACE, "forking worker\n");
        next_shm_index = get_next_shm_index();
    }

    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    /* fork a child process */
    pid=fork();

    /* an error occurred while trying to fork */
    if(pid==-1){
        perror("fork");
        gm_log( GM_LOG_ERROR, "fork error\n" );
        return GM_ERROR;
    }

    /* we are in the child process */
    else if(pid==0){

        gm_log( GM_LOG_DEBUG, "child started with pid: %d\n", getpid() );
        shm[next_shm_index] = -getpid();

        /* do the real work */
#ifdef EMBEDDEDPERL
        worker_client(mode, next_shm_index, shmid, start_env);
#else
        worker_client(mode, next_shm_index, shmid);
#endif

        exit(EXIT_SUCCESS);
    }

    /* parent  */
    else if(pid > 0){
        signal(SIGINT, clean_exit);
        signal(SIGTERM,clean_exit);
        shm[next_shm_index] = -pid;
    }

    return GM_OK;
}


/* parse command line arguments */
int parse_arguments(int argc, char **argv) {
    int i;
    int errors = 0;
    int verify;
    mod_gm_opt_t * mod_gm_new_opt;
    mod_gm_new_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_new_opt);
    for(i=1;i<argc;i++) {
        char * arg   = strdup( argv[i] );
        char * arg_c = arg;
        if ( !strcmp( arg, "version" ) || !strcmp( arg, "--version" )  || !strcmp( arg, "-V" ) ) {
            print_version();
        }
        if ( !strcmp( arg, "help" ) || !strcmp( arg, "--help" )  || !strcmp( arg, "-h" ) ) {
            print_usage();
        }
        if(parse_args_line(mod_gm_new_opt, arg, 0) != GM_OK) {
            errors++;
            free(arg_c);
            break;
        }
        free(arg_c);
    }

    /* set identifier to hostname unless specified */
    if(mod_gm_new_opt->identifier == NULL) {
        gethostname(hostname, GM_BUFFERSIZE-1);
        mod_gm_new_opt->identifier = strdup(hostname);
    }

    /* close old logfile */
    if(mod_gm_opt->logfile_fp != NULL) {
        fclose(mod_gm_opt->logfile_fp);
        mod_gm_opt->logfile_fp = NULL;
    }

    /* verify options */
    verify = verify_options(mod_gm_new_opt);

    /* set new options */
    if(errors == 0 && verify == GM_OK) {
        mod_gm_free_opt(mod_gm_opt);
        mod_gm_opt = mod_gm_new_opt;
    }

    /* open new logfile */
    if ( mod_gm_new_opt->logmode == GM_LOG_MODE_AUTO && mod_gm_new_opt->logfile ) {
        mod_gm_opt->logmode = GM_LOG_MODE_FILE;
    }
    if(mod_gm_new_opt->logmode == GM_LOG_MODE_FILE && mod_gm_opt->logfile && mod_gm_opt->debug_level < GM_LOG_STDOUT) {
        mod_gm_opt->logfile_fp = fopen(mod_gm_opt->logfile, "a+");
        if(mod_gm_opt->logfile_fp == NULL) {
            perror(mod_gm_opt->logfile);
            errors++;
        }
    }

    /* read keyfile */
    if(mod_gm_opt->keyfile != NULL && read_keyfile(mod_gm_opt) != GM_OK) {
        errors++;
    }

    if(verify != GM_OK || errors > 0 || mod_gm_new_opt->debug_level >= GM_LOG_DEBUG) {
        int old_debug = mod_gm_opt->debug_level;
        mod_gm_opt->debug_level = GM_LOG_DEBUG;
        dumpconfig(mod_gm_new_opt, GM_WORKER_MODE);
        mod_gm_opt->debug_level = old_debug;
    }

    if(errors > 0 || verify != GM_OK) {
        mod_gm_free_opt(mod_gm_new_opt);
        return(GM_ERROR);
    }

    return(GM_OK);
}


/* verify our option */
int verify_options(mod_gm_opt_t *opt) {

    /* stdout loggin in daemon mode is pointless */
    if( opt->debug_level > GM_LOG_TRACE && opt->daemon_mode == GM_ENABLED) {
        opt->debug_level = GM_LOG_TRACE;
    }

    /* did we get any server? */
    if(opt->server_num == 0) {
        gm_log( GM_LOG_ERROR, "please specify at least one server\n" );
        return(GM_ERROR);
    }

    /* nothing set by hand -> defaults */
    if( opt->set_queues_by_hand == 0 ) {
        gm_log( GM_LOG_DEBUG, "starting client with default queues\n" );
        opt->hosts    = GM_ENABLED;
        opt->services = GM_ENABLED;
        opt->events   = GM_ENABLED;
    }

    /* do we have queues to serve? */
    if(   opt->servicegroups_num == 0
       && opt->hostgroups_num    == 0
       && opt->hosts    == GM_DISABLED
       && opt->services == GM_DISABLED
       && opt->events   == GM_DISABLED
      ) {
        gm_log( GM_LOG_ERROR, "starting worker without any queues is useless\n" );
        return(GM_ERROR);
    }

    if(opt->min_worker > opt->max_worker)
        opt->min_worker = opt->max_worker;

    /* encryption without key? */
    if(opt->encryption == GM_ENABLED) {
        if(opt->crypt_key == NULL && opt->keyfile == NULL) {
            gm_log( GM_LOG_ERROR, "no encryption key provided, please use --key=... or keyfile=... or disable encryption\n");
            return(GM_ERROR);
        }
    }

    return(GM_OK);
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("worker [ --debug=<lvl>                            ]\n");
    printf("       [ --logmode=<automatic|stdout|syslog|file> ]\n");
    printf("       [ --logfile=<path>                         ]\n");
    printf("       [ --debug-result                           ]\n");
    printf("       [ --help|-h                                ]\n");
    printf("       [ --daemon|-d                              ]\n");
    printf("       [ --config=<configfile>                    ]\n");
    printf("       [ --server=<server>                        ]\n");
    printf("       [ --dupserver=<server>                     ]\n");
    printf("\n");
    printf("       [ --hosts               ]\n");
    printf("       [ --services            ]\n");
    printf("       [ --eventhandler        ]\n");
    printf("       [ --hostgroup=<name>    ]\n");
    printf("       [ --servicegroup=<name> ]\n");
    printf("       [ --do_hostchecks       ]\n");
    printf("\n");
    printf("       [ --min-worker=<nr>     ]\n");
    printf("       [ --max-worker=<nr>     ]\n");
    printf("\n");
    printf("       [ --max-age=<sec>       ]\n");
    printf("       [ --timeout             ]\n");
    printf("\n");
    printf("       [ --encryption=<yes|no> ]\n");
    printf("       [ --key=<string>        ]\n");
    printf("       [ --keyfile=<file>      ]\n");
    printf("\n");
    printf("       [ --min-worker=<nr>     ]\n");
    printf("       [ --max-worker=<nr>     ]\n");
    printf("       [ --idle-timeout=<nr>   ]\n");
    printf("       [ --max-jobs=<nr>       ]\n");
    printf("       [ --spawn-rate=<nr>     ]\n");
    printf("       [ --fork_on_exec        ]\n");
    printf("       [ --show_error_output   ]\n");
    printf("\n");
    printf("       [ --enable_embedded_perl         ]\n");
    printf("       [ --use_embedded_perl_implicitly ]\n");
    printf("       [ --use_perl_cache               ]\n");
    printf("       [ --p1_file                      ]\n");
    printf("\n");
    printf("       [ --workaround_rc_25    ]\n");
    printf("\n");
    printf("see README for a detailed explaination of all options.\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* create shared memory segments */
void setup_child_communicator() {
    int x;

    gm_log( GM_LOG_TRACE, "setup_child_communicator()\n");

    /* Create the segment. */
    mod_gm_shm_key = getpid(); /* use pid as shm key */
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, IPC_CREAT | 0600)) < 0) {
        perror("shmget");
        exit( EXIT_FAILURE );
    }

    /* Now we attach the segment to our data space. */
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit( EXIT_FAILURE );
    }
    shm[0] = 0;  /* done jobs         */
    shm[1] = 0;  /* total worker      */
    shm[2] = 0;  /* running worker    */
    shm[3] = -1; /* status worker pid */
    for(x = 0; x < mod_gm_opt->max_worker; x++) {
        shm[x+4] = -1; /* normal worker   */
    }

    return;
}


/* set new number of workers */
int adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs) {
    int perc_running;
    int idle;
    int target = min;

    if(cur_workers == 0) {
        gm_log( GM_LOG_TRACE3, "adjust_number_of_worker(min %d, max %d, worker %d, jobs %d) -> %d\n", min, max, cur_workers, cur_jobs, mod_gm_opt->min_worker);
        return mod_gm_opt->min_worker;
    }

    perc_running = (int)cur_jobs*100/cur_workers;
    idle         = (int)cur_workers - cur_jobs;

    gm_log( GM_LOG_TRACE3, "adjust_number_of_worker(min %d, max %d, worker %d, jobs %d) = %d%% running\n", min, max, cur_workers, cur_jobs, perc_running);

    if(cur_workers == max)
        return max;

    /* > 90% workers running */
    if(cur_jobs > 0 && ( perc_running > 90 || idle <= 2 )) {
        /* increase target number by 2 */
        gm_log( GM_LOG_TRACE, "starting %d new workers\n", mod_gm_opt->spawn_rate);
        target = cur_workers + mod_gm_opt->spawn_rate;
    }

    /* dont go over the top */
    if(target > max) { target = max; }

    if(target != cur_workers)
        gm_log( GM_LOG_TRACE3, "adjust_number_of_worker(min %d, max %d, worker %d, jobs %d) = %d%% running -> %d\n", min, max, cur_workers, cur_jobs, perc_running, target);

    return target;
}


/* do a clean exit */
void clean_exit(int sig) {
    gm_log( GM_LOG_TRACE, "clean_exit(%d)\n", sig);

    if(mod_gm_opt->pidfile != NULL)
        unlink(mod_gm_opt->pidfile);

    /* stop all childs */
    stop_childs(GM_WORKER_STOP);

    /* detach shm */
    if(shmdt(shm) < 0)
        perror("shmdt");

    gm_log( GM_LOG_INFO, "mod_gearman worker exited\n");
    mod_gm_free_opt(mod_gm_opt);
    exit( EXIT_SUCCESS );
}


/* stop all childs */
void stop_childs(int mode) {
    int status, chld;
    int waited = 0;
    int x, curpid;

    gm_log( GM_LOG_TRACE, "stop_childs(%d)\n", mode);

    /* ignore some signals for now */
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT,  SIG_IGN);

    /*
     * send term signal to our childs
     * children will finish the current job and exit
     */
    killpg(0, SIGTERM);
    while(current_number_of_workers > 0) {
        gm_log( GM_LOG_TRACE, "send SIGTERM\n");
        for(x=3; x < mod_gm_opt->max_worker+4; x++) {
            curpid = shm[x];
            if(curpid < 0) { curpid = -curpid; }
            if( curpid != 0 ) {
                kill(curpid, SIGTERM);
            }
        }
        while((chld = waitpid(-1, &status, WNOHANG)) != -1 && chld > 0) {
            gm_log( GM_LOG_TRACE, "wait() %d exited with %d\n", chld, status);
        }
        sleep(1);
        waited++;
        if(waited > GM_CHILD_SHUTDOWN_TIMEOUT) {
            break;
        }
        count_current_worker(GM_DISABLED);
        if(current_number_of_workers == 0)
            return;
        gm_log( GM_LOG_TRACE, "still waiting (%d) %d childs missing...\n", waited, current_number_of_workers);
    }

    if(mode == GM_WORKER_STOP) {
        killpg(0, SIGINT);
        count_current_worker(GM_DISABLED);
        if(current_number_of_workers == 0)
            return;

        gm_log( GM_LOG_TRACE, "sending SIGINT...\n");
        for(x=3; x < mod_gm_opt->max_worker+4; x++) {
            curpid = shm[x];
            if(curpid < 0) { curpid = -curpid; }
            if( curpid != 0 ) {
                kill(curpid, SIGINT);
            }
        }

        while((chld = waitpid(-1, &status, WNOHANG)) != -1 && chld > 0) {
            gm_log( GM_LOG_TRACE, "wait() %d exited with %d\n", chld, status);
        }

        /* kill them the hard way */
        count_current_worker(GM_DISABLED);
        if(current_number_of_workers == 0)
            return;
        for(x=3; x < mod_gm_opt->max_worker+4; x++) {
            if( shm[x] != 0 ) {
                curpid = shm[x];
                if(curpid < 0) { curpid = -curpid; }
                if( curpid != 0 ) {
                    kill(curpid, SIGKILL);
                }
            }
        }

        /* count childs a last time */
        count_current_worker(GM_DISABLED);
        if(current_number_of_workers == 0)
            return;

        /*
         * clean up shared memory
         * will be removed when last client detaches
         */
        if( shmctl( shmid, IPC_RMID, 0 ) == -1 ) {
            perror("shmctl");
        } else {
            gm_log( GM_LOG_TRACE, "shared memory deleted\n");
        }

        /* this will kill us too */
        gm_log( GM_LOG_ERROR, "exiting by SIGKILL...\n");
        killpg(0, SIGKILL);
    }

    /* restore signal handlers for a clean exit */
    signal(SIGINT, clean_exit);
    signal(SIGTERM,clean_exit);
}


/* check for pid file and write new one */
int write_pid_file() {
    FILE *fp;
    char pid_path[GM_BUFFERSIZE];

    /* no pidfile given */
    if(mod_gm_opt->pidfile == NULL)
        return GM_OK;

    if(file_exists(mod_gm_opt->pidfile)) {
        fp = fopen(mod_gm_opt->pidfile, "r");
        if(fp != NULL) {
            char *pid;
            pid = malloc(GM_BUFFERSIZE);
            if(fgets(pid, GM_BUFFERSIZE, fp) == NULL)
                perror("fgets");
            fclose(fp);
            pid = trim(pid);
            gm_log( GM_LOG_INFO, "found pid file for: %s\n", pid);
            snprintf(pid_path, GM_BUFFERSIZE, "/proc/%s/status", pid);
            free(pid);
            if(file_exists(pid_path)) {
                gm_log( GM_LOG_INFO, "pidfile already exists, cannot start!\n");
                return(GM_ERROR);
            } else {
                gm_log( GM_LOG_INFO, "removed stale pidfile\n");
                unlink(mod_gm_opt->pidfile);
            }
        } else {
            perror(mod_gm_opt->pidfile);
            gm_log( GM_LOG_INFO, "cannot read pidfile\n");
            return(GM_ERROR);
        }
    }

    /* now write new pidfile */
    fp = fopen(mod_gm_opt->pidfile,"w+");
    if(fp == NULL) {
        perror(mod_gm_opt->pidfile);
        gm_log( GM_LOG_ERROR, "cannot write pidfile\n");
        return(GM_ERROR);
    }

    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    gm_log( GM_LOG_DEBUG, "pid file %s written\n", mod_gm_opt->pidfile );
    return GM_OK;
}


/* store the original command line for later reloads */
int store_original_comandline(int argc, char **argv) {
    orig_argc = argc;
    orig_argv = argv;
    return(GM_OK);
}


/* try to reload the config */
void reload_config(int sig) {
    gm_log( GM_LOG_TRACE, "reload_config(%d)\n", sig);
    if(parse_arguments(orig_argc, orig_argv) != GM_OK) {
        gm_log( GM_LOG_ERROR, "reload config failed, check your config\n");
        return;
    }

    /*
     * restart workers gracefully:
     * send term signal to our childs
     * children will finish the current job and exit
     */
    stop_childs(GM_WORKER_RESTART);

    /* start status worker */
    make_new_child(GM_WORKER_STATUS);

    gm_log( GM_LOG_INFO, "reloading config was successful\n");

    return;
}


/* return and reserve next shm index*/
int get_next_shm_index() {
    int x;
    int next_index = 0;

    gm_log( GM_LOG_TRACE, "get_next_shm_index()\n" );

    for(x = 4; x < mod_gm_opt->max_worker+4; x++) {
        if(shm[x] == -1) {
            next_index      = x;
            shm[next_index] = 1;
            break;
        }
    }

    if(next_index == 0) {
        gm_log(GM_LOG_ERROR, "unable to get next shm id\n");
        clean_exit(15);
        exit(EXIT_FAILURE);
    }
    gm_log( GM_LOG_TRACE, "get_next_shm_index() -> %d\n", next_index );

    return next_index;
}


/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for worker: %s", data);
    return;
}

/* print version */
void print_version() {
    printf("mod_gearman_worker: version %s running on libgearman %s\n", GM_VERSION, gearman_version());
    printf("\n");
    exit( STATE_UNKNOWN );
}
