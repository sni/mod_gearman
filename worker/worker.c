/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* include header */
#include "worker.h"
#include "utils.h"
#include "worker_logger.h"
#include "worker_client.h"

int current_number_of_workers                = 0;
volatile sig_atomic_t current_number_of_jobs = 0;  // must be signal safe

int     orig_argc;
char ** orig_argv;

/* work starts here */
int main (int argc, char **argv) {

    /* store the original command line for later reloads */
    store_original_comandline(argc, argv);

    /*
     * allocate options structure
     * and parse command line
     */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    signal(SIGHUP, reload_config);
    if(parse_arguments(argc, argv) != GM_OK) {
        exit( EXIT_FAILURE );
    }

    /* fork into daemon mode? */
    if(mod_gm_opt->daemon_mode == GM_ENABLED) {
        pid_t pid = fork();
        /* an error occurred while trying to fork */
        if(pid == -1) {
            perror("fork");
            exit( EXIT_FAILURE );
        }
        /* we are in the child process */
        else if(pid == 0) {
            logger( GM_LOG_INFO, "mod_gearman worker daemon started with pid %d\n", getpid());
        }
        /* we are the parent. So forking into daemon mode worked */
        else {
            exit( EXIT_SUCCESS );
        }
    }

    /* set signal handlers for a clean exit */
    signal(SIGINT, clean_exit);
    signal(SIGTERM,clean_exit);


    /* check and write pid file */
    if(write_pid_file() != GM_OK) {
        exit( EXIT_SUCCESS );
    }

    /* init crypto functions */
    if(mod_gm_opt->encryption == GM_ENABLED) {
        if(mod_gm_opt->crypt_key == NULL) {
            logger( GM_LOG_ERROR, "no encryption key provided, please use --key=... or keyfile=...\n");
            exit( EXIT_FAILURE );
        }
        mod_gm_crypt_init(mod_gm_opt->crypt_key);
    } else {
        mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    }

    logger( GM_LOG_DEBUG, "main process started\n");

    if(mod_gm_opt->max_worker == 1) {
        worker_client(GM_WORKER_STANDALONE);
        exit( EXIT_SUCCESS );
    }

    setup_child_communicator();

    // create initial childs
    int x;
    for(x=0; x < mod_gm_opt->min_worker; x++) {
        make_new_child();
    }

    // maintain the population
    int now                 = (int)time(NULL);
    int last_time_increased = now;
    int had_to_increase     = 0;
    while (1) {
        // check number of workers every 30 seconds
        // sleep gets canceled anyway when receiving signals
        sleep(30);

        // collect finished workers
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0) {
            current_number_of_workers--;
            logger( GM_LOG_TRACE, "waitpid() %d\n", status);
        }

        if(current_number_of_jobs < 0) { current_number_of_jobs = 0; }
        if(current_number_of_jobs > current_number_of_workers) { current_number_of_jobs = current_number_of_workers; }

        // keep up minimum population
        for (x = current_number_of_workers; x < mod_gm_opt->min_worker; x++) {
            make_new_child();
        }

        had_to_increase = 0;
        now = (int)time(NULL);
        if(last_time_increased +2 > now)
            continue;

        int target_number_of_workers = adjust_number_of_worker(mod_gm_opt->min_worker, mod_gm_opt->max_worker, current_number_of_workers, current_number_of_jobs);
        for (x = current_number_of_workers; x < target_number_of_workers; x++) {
            // top up the worker pool
            make_new_child();
            had_to_increase = 1;
        }

        /* wait a little bit, otherwise worker would be spawned really fast */
        if(had_to_increase) {
            last_time_increased = time(NULL);
        }
    }

    clean_exit(15);
    exit( EXIT_SUCCESS );
}


/* start up new worker */
int make_new_child() {
    logger( GM_LOG_TRACE, "make_new_child()\n");
    pid_t pid = 0;

    /* fork a child process */
    pid=fork();

    /* an error occurred while trying to fork */
    if(pid==-1){
        perror("fork");
        logger( GM_LOG_ERROR, "fork error\n" );
        return GM_ERROR;
    }

    /* we are in the child process */
    else if(pid==0){
        logger( GM_LOG_DEBUG, "worker started with pid: %d\n", getpid() );

        signal(SIGUSR1, SIG_IGN);
        signal(SIGINT,  SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        // do the real work
        worker_client(GM_WORKER_MULTI);

        exit(EXIT_SUCCESS);
    }

    /* parent  */
    else if(pid > 0){
        current_number_of_workers++;
    }

    return GM_OK;
}


/* parse command line arguments */
int parse_arguments(int argc, char **argv) {
    if(mod_gm_opt->logfile_fp != NULL) {
        fclose(mod_gm_opt->logfile_fp);
        mod_gm_opt->logfile_fp = NULL;
    }
    int i;
    int errors = 0;
    mod_gm_opt_t * mod_gm_new_opt;
    mod_gm_new_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_new_opt);
    for(i=1;i<argc;i++) {
        char * arg   = strdup( argv[i] );
        lc(arg);
        if ( !strcmp( arg, "help" ) || !strcmp( arg, "--help" )  || !strcmp( arg, "-h" ) ) {
            print_usage();
        }
        if(parse_args_line(mod_gm_new_opt, arg, 0) != GM_OK) {
            errors++;
            free(arg);
            break;
        }
        free(arg);
    }

    int verify;
    verify = verify_options(mod_gm_new_opt);
    if(errors == 0 && verify == GM_OK) {
        mod_gm_free_opt(mod_gm_opt);
        mod_gm_opt = mod_gm_new_opt;
    }

    if(mod_gm_opt->logfile) {
        mod_gm_opt->logfile_fp = fopen(mod_gm_opt->logfile, "a+");
        if(mod_gm_opt->logfile_fp == NULL) {
            perror(mod_gm_opt->logfile);
        }
    }

    /* read keyfile */
    read_keyfile(mod_gm_opt);
    if(verify != GM_OK || errors > 0 || mod_gm_new_opt->debug_level >= GM_LOG_DEBUG) {
        int old_debug = mod_gm_opt->debug_level;
        mod_gm_opt->debug_level = GM_LOG_DEBUG;
        dumpconfig(mod_gm_new_opt, GM_WORKER_MODE);
        mod_gm_opt->debug_level = old_debug;
    }

    if(errors > 0) {
        return(GM_ERROR);
    }

    return(verify);
}


/* verify our option */
int verify_options(mod_gm_opt_t *opt) {
    // did we get any server?
    if(opt->server_num == 0) {
        logger( GM_LOG_ERROR, "please specify at least one server\n" );
        return(GM_ERROR);
    }

    // nothing set by hand -> defaults
    if( opt->set_queues_by_hand == 0 ) {
        logger( GM_LOG_DEBUG, "starting client with default queues\n" );
        opt->hosts    = GM_ENABLED;
        opt->services = GM_ENABLED;
        opt->events   = GM_ENABLED;
    }

    if(   opt->servicegroups_num == 0
       && opt->hostgroups_num    == 0
       && opt->hosts    == GM_DISABLED
       && opt->services == GM_DISABLED
       && opt->events   == GM_DISABLED
      ) {
        logger( GM_LOG_ERROR, "starting worker without any queues is useless\n" );
        return(GM_ERROR);
    }

    if(opt->min_worker > opt->max_worker)
        opt->min_worker = opt->max_worker;

    return(GM_OK);
}


/* print usage */
void print_usage() {
    printf("usage:\n");
    printf("\n");
    printf("worker [ --debug=<lvl>         ]\n");
    printf("       [ --debug-result        ]\n");
    printf("       [ --help|-h             ]\n");
    printf("       [ --daemon|-d           ]\n");
    printf("\n");
    printf("       [ --config=<configfile> ]\n");
    printf("\n");
    printf("       [ --server=<server>     ]\n");
    printf("\n");
    printf("       [ --hosts               ]\n");
    printf("       [ --services            ]\n");
    printf("       [ --events              ]\n");
    printf("       [ --hostgroup=<name>    ]\n");
    printf("       [ --servicegroup=<name> ]\n");
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
    printf("see README for a detailed explaination of all options.\n");
    printf("\n");

    exit( EXIT_SUCCESS );
}


/* check child signal pipe */
void check_signal(int sig) {
    logger( GM_LOG_TRACE, "check_signal(%i)\n", sig);

    int shmid;
    int *shm;

    // Locate the segment.
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    // Now we attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }

    logger( GM_LOG_TRACE, "check_signal: %i\n", shm[0]);
    current_number_of_jobs = shm[0];

    // detach from shared memory
    if(shmdt(shm) < 0)
        perror("shmdt");

    return;
}

/* create shared memory segments */
void setup_child_communicator() {
    logger( GM_LOG_TRACE, "setup_child_communicator()\n");

    // setup signal handler
    struct sigaction usr1_action;
    sigset_t block_mask;
    sigfillset (&block_mask); // block all signals
    usr1_action.sa_handler = check_signal;
    usr1_action.sa_mask    = block_mask;
    usr1_action.sa_flags   = 0;
    sigaction (SIGUSR1, &usr1_action, NULL);

    int shmid;
    int * shm;

    // Create the segment.
    mod_gm_shm_key = getpid(); // use pid as shm key
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    // Now we attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (int *) -1) {
        perror("shmat");
        exit(1);
    }
    shm[0] = 0;
    logger( GM_LOG_TRACE, "setup: %i\n", shm[0]);

    // detach from shared memory
    if(shmdt(shm) < 0)
        perror("shmdt");

    return;
}


/* set new number of workers */
int adjust_number_of_worker(int min, int max, int cur_workers, int cur_jobs) {
    int perc_running = (int)cur_jobs*100/cur_workers;
    int idle         = (int)cur_workers - cur_jobs;
    logger( GM_LOG_TRACE, "adjust_number_of_worker(min %d, max %d, worker %d, jobs %d) = %d%% running\n", min, max, cur_workers, cur_jobs, perc_running);
    int target = min;

    if(cur_workers == max)
        return max;

    // > 90% workers running
    if(cur_jobs > 0 && ( perc_running > 90 || idle <= 2 )) {
        // increase target number by 2
        logger( GM_LOG_TRACE, "starting 2 new workers\n");
        target = cur_workers + 2;
    }

    // dont go over the top
    if(target > max) { target = max; }

    return target;
}


/* do a clean exit */
void clean_exit(int sig) {
    logger( GM_LOG_TRACE, "clean_exit(%d)\n", sig);

    /* stop all childs */
    stop_childs(GM_WORKER_STOP);

    /*
     * clean up shared memory
     * will be removed when last client detaches
     */
    int shmid;
    if ((shmid = shmget(mod_gm_shm_key, GM_SHM_SIZE, 0666)) < 0) {
        perror("shmget");
    }
    if( shmctl( shmid, IPC_RMID, 0 ) == -1 ) {
        perror("shmctl");
    }
    logger( GM_LOG_TRACE, "shared memory deleted\n");
    sleep(1);

    /* kill remaining worker */
    if(current_number_of_workers > 0) {
        pid_t pid = getpid();
        kill(-pid, SIGKILL);
    }

    if(mod_gm_opt->pidfile != NULL)
        unlink(mod_gm_opt->pidfile);

    logger( GM_LOG_INFO, "mod_gearman worker exited\n");

    mod_gm_free_opt(mod_gm_opt);

    exit( EXIT_SUCCESS );
}


/* stop all childs */
void stop_childs(int mode) {
    // become the process group leader
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT,  SIG_DFL);

    /*
     * send term signal to our childs
     * children will finish the current job and exit
     */
    pid_t pid = getpid();
    logger( GM_LOG_TRACE, "send SIGTERM to %d\n", pid);
    kill(-pid, SIGTERM);
    signal(SIGTERM, SIG_DFL);

    logger( GM_LOG_TRACE, "waiting for childs to exit...\n");
    int status, chld;
    int waited = 0;

    while(current_number_of_workers > 0) {
        while((chld = waitpid(-1, &status, WNOHANG)) != -1) {
            if(chld > 0) {
                current_number_of_workers--;
                logger( GM_LOG_TRACE, "wait() %d exited with %d\n", chld, status);
                if(mode == GM_WORKER_RESTART) {
                    make_new_child();
                }
            }
        }
        sleep(1);
        waited++;
        if(waited > GM_CHILD_SHUTDOWN_TIMEOUT) {
            break;
        }
        logger( GM_LOG_TRACE, "still waiting (%d)...\n", waited);
    }

    if(mode == GM_WORKER_STOP) {
        if(current_number_of_workers > 0) {
            logger( GM_LOG_TRACE, "sending SIGINT...\n");
            kill(-pid, SIGINT);
        }

        /* this will kill us too */
        while(waitpid(-1, &status, WNOHANG) > 0) {
            current_number_of_workers--;
            logger( GM_LOG_TRACE, "wait() %d exited with %d\n", chld, status);
        }
    }
}


/* check for pid file and write new one */
int write_pid_file() {
    FILE *fp;

    /* no pidfile given */
    if(mod_gm_opt->pidfile == NULL)
        return GM_OK;

    if(file_exists(mod_gm_opt->pidfile)) {
        fp = fopen(mod_gm_opt->pidfile, "r");
        if(fp != NULL) {
            char *pid;
            pid = malloc(GM_BUFFERSIZE);
            fgets(pid, GM_BUFFERSIZE, fp);
            fclose(fp);
            pid = trim(pid);
            logger( GM_LOG_INFO, "found pid file for: %s\n", pid);
            char pid_path[GM_BUFFERSIZE];
            snprintf(pid_path, GM_BUFFERSIZE, "/proc/%s/status", pid);
            if(file_exists(pid_path)) {
                logger( GM_LOG_INFO, "pidfile already exists, cannot start!\n");
                return(GM_ERROR);
            } else {
                logger( GM_LOG_INFO, "removed stale pidfile\n");
                unlink(mod_gm_opt->pidfile);
            }
        } else {
            perror(mod_gm_opt->pidfile);
            logger( GM_LOG_INFO, "cannot read pidfile\n");
            return(GM_ERROR);
        }
    }

    /* now write new pidfile */
    fp = fopen(mod_gm_opt->pidfile,"w+");
    if(fp == NULL)
        perror("fopen");

    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    logger( GM_LOG_DEBUG, "pid file %s written\n", mod_gm_opt->pidfile );
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
    logger( GM_LOG_TRACE, "reload_config(%d)\n", sig);
    if(parse_arguments(orig_argc, orig_argv) != GM_OK) {
        logger( GM_LOG_ERROR, "reload config failed, check your config\n");
        return;
    }

    /*
     * restart workers gracefully:
     * send term signal to our childs
     * children will finish the current job and exit
     */
    stop_childs(GM_WORKER_RESTART);

    logger( GM_LOG_INFO, "reloading config was successful\n");

    return;
}
