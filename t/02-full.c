#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <config.h>
#include <t/tap.h>
#include <common.h>
#include <utils.h>
#include <check_utils.h>
#ifdef EMBEDDEDPERL
#include <epn_utils.h>
#endif
#include "gearman_utils.h"

#define GEARMAND_TEST_PORT   54730

#include <worker_dummy_functions.c>

char * worker_logfile;
int gearmand_pid;
int worker_pid;
extern gearman_worker_st *worker;
extern gearman_client_st *client;
mod_gm_opt_t *mod_gm_opt;
char * last_result;
char hostname[GM_SMALLBUFSIZE];
volatile sig_atomic_t shmid;
EVP_CIPHER_CTX * test_ctx = NULL;

/* start the gearmand server */
void *start_gearmand(void*data);
void *start_gearmand(__attribute__((unused)) void*data) {
    pid_t pid = fork();
    if(pid == 0) {
        setsid();
        char port[30];
        snprintf(port, 30, "--port=%d", GEARMAND_TEST_PORT);
        /* for newer gearman versions */
        if(atof(gearman_version()) >= 0.27) {
            execlp("gearmand", "gearmand", "--listen=127.0.0.1", "--threads=10", "--job-retries=0", port, "--verbose=DEBUG", "--log-file=/tmp/gearmand.log" , (char *)NULL);
        } else if(atof(gearman_version()) > 0.14) {
            execlp("gearmand", "gearmand", "--listen=127.0.0.1", "--threads=10", "--job-retries=0", port, "--verbose=999", "--log-file=/tmp/gearmand.log" , (char *)NULL);
        } else {
            /* for gearman 0.14 */
            execlp("gearmand", "gearmand", "-t 10", "-j 0", port, (char *)NULL);
        }
        perror("gearmand");
        exit(1);
    }
    else {
        gearmand_pid = pid;
    }
    return NULL;
}


/* start the test worker */
void *start_worker(void*data);
void *start_worker(void*data) {
    char* key = (char*)data;
    pid_t pid = fork();
    if(pid == 0) {
        setsid();
        char options[150];
        snprintf(options, 150, "server=127.0.0.1:%d", GEARMAND_TEST_PORT);
        char logf[150];
        snprintf(logf, 150, "logfile=%s", worker_logfile);
        if(key != NULL) {
            char encryption[150];
            snprintf(encryption, 150, "key=%s", key);
            execl("./mod_gearman_worker", "./mod_gearman_worker", "debug=2", encryption,      logf, "max-worker=1", "p1_file=./worker/mod_gearman_p1.pl", options, (char *)NULL);
        } else {
            execl("./mod_gearman_worker", "./mod_gearman_worker", "debug=2", "encryption=no", logf, "max-worker=1", "p1_file=./worker/mod_gearman_p1.pl", options, (char *)NULL);
        }
        perror("mod_gearman_worker");
        exit(1);
    }
    else {
        worker_pid = pid;
    }
    return NULL;
}


/* test event handler over gearmand */
void test_eventhandler(int transportmode);
void test_eventhandler(int transportmode) {
    char * testdata = strdup("type=eventhandler\ncommand_line=/bin/hostname\n\n\n");
    int rt = add_job_to_queue(&client, mod_gm_opt->server_list, "eventhandler", NULL, testdata, GM_JOB_PRIO_NORMAL, 1, transportmode, test_ctx, 0, 1);
    ok(rt == GM_OK, "eventhandler sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    gm_free(testdata);
    return;
}


/* test service check handler over gearmand */
void test_servicecheck(int transportmode, char*cmd);
void test_servicecheck(int transportmode, char*cmd) {
    struct timeval start_time;
    gettimeofday(&start_time,NULL);
    char temp_buffer[GM_BUFFERSIZE];
    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%Lf\ntimeout=%d\ncheck_options=%i\nscheduled_check=%i\nlatency=%f\ncommand_line=%s\n\n\n",
              GM_DEFAULT_RESULT_QUEUE,
              "host1",
              "service1",
              timeval2double(&start_time),
              60,
              0,
              1,
              0.0,
              cmd==NULL ? "/bin/hostname" : cmd
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';
    int rt = add_job_to_queue(&client, mod_gm_opt->server_list, "service", NULL, temp_buffer, GM_JOB_PRIO_NORMAL, 1, transportmode, test_ctx, 0, 1);
    ok(rt == GM_OK, "servicecheck sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    return;
}

/* test */
void send_big_jobs(int transportmode);
void send_big_jobs(int transportmode) {
    struct timeval start_time;
    char uniq[GM_SMALLBUFSIZE];
    gettimeofday(&start_time,NULL);
    char temp_buffer[GM_BUFFERSIZE];
    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%Lf\ntimeout=%d\ncheck_options=%i\nscheduled_check=%i\nlatency=%f\ncommand_line=%s\n\n\n",
              GM_DEFAULT_RESULT_QUEUE,
              "host1",
              "service1",
              timeval2double(&start_time),
              60,
              0,
              1,
              0.0,
              "/bin/hostname"
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';
    make_uniq(uniq, "%s", "something at least bigger than the (GEARMAN_MAX_UNIQUE_SIZE) 64 chars allowed by libgearman!");
    int rt = add_job_to_queue(&client, mod_gm_opt->server_list, "service", uniq, temp_buffer, GM_JOB_PRIO_NORMAL, 1, transportmode, test_ctx, 0, 1);
    ok(rt == GM_OK, "big uniq id sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");

    char * queue = "something at least bigger than the (GEARMAN_FUNCTION_MAX_SIZE) 512 chars allowed by libgearman! aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    rt = add_job_to_queue(&client, mod_gm_opt->server_list, queue, uniq, temp_buffer, GM_JOB_PRIO_NORMAL, 1, transportmode, test_ctx, 0, 1);
    ok(rt == GM_ERROR, "big queue sent unsuccessfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");

    return;
}

/* put back the result into the core */
void *get_results( gearman_job_st *job, void *context, size_t *result_size, gearman_return_t *ret_ptr );
void *get_results( gearman_job_st *job, __attribute__((unused)) void *context, size_t *result_size, gearman_return_t *ret_ptr ) {
    size_t wsize;
    const char *workload;
    char *decrypted_data = NULL;

    /* set size of result */
    *result_size = 0;

    /* set result pointer to success */
    *ret_ptr = GEARMAN_SUCCESS;

    /* get the data */
    wsize = gearman_job_workload_size(job);
    workload = (const char *)gearman_job_workload(job);
    if(workload == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }

    /* decrypt data */
    mod_gm_decrypt(test_ctx, &decrypted_data, workload, wsize, mod_gm_opt->transportmode);

    if(decrypted_data == NULL) {
        *ret_ptr = GEARMAN_WORK_FAIL;
        return NULL;
    }

    like(decrypted_data, "host_name=host1", "output contains host_name");
    like(decrypted_data, "output=", "output contains output");

    if(last_result != NULL)
        gm_free(last_result);
    last_result = decrypted_data;

    return NULL;
}

/* create server / clients / worker */
void create_modules(void);
void create_modules() {
    client = create_client(mod_gm_opt->server_list);
    ok(client != NULL, "created test client");

    worker = create_worker( mod_gm_opt->server_list);
    ok(worker != NULL, "created test worker");
    ok(worker_add_function( worker, GM_DEFAULT_RESULT_QUEUE, get_results ) == GM_OK, "added result worker");
    gearman_worker_set_timeout(worker, 1000);
    return;
}

/* grab one job from result queue */
void do_result_work(int);
void do_result_work(int nr) {
    gearman_return_t ret;
    int x = 0;
    for(x=0;x<nr;x++) {
        ret = gearman_worker_work( worker );
        ok(ret == GEARMAN_SUCCESS, "got valid job from result queue" );
    }
    return;
}


/* check logfile for errors
 * mode:
 *       1 to display complete file by diag()
 *       2 to display only errors
 *       3 to display even without errors
 */
void check_logfile(char *logfile, int mode);
void check_logfile(char *logfile, int mode) {
    FILE * fp;
    char *line;
    int errors = 0;

    fp = fopen(logfile, "r");
    if(fp == NULL) {
        perror(logfile);
        return;
    }
    line = malloc(GM_BUFFERSIZE);
    while(fgets(line, GM_BUFFERSIZE, fp) != NULL) {
        if(strstr(line, "ERROR") != NULL) {
            if(mode == 2)
                diag("logfile: %s", line);
            errors++;
        }
    }
    fclose(fp);

    /* output complete logfile */
    if((errors > 0 && mode == 1) || mode == 3) {
        fp = fopen(logfile, "r");
        while(fgets(line, GM_BUFFERSIZE, fp) != NULL) {
            diag("logfile: %s", line);
        }
        fclose(fp);
    }

    ok(errors == 0, "errors in logfile: %d", errors);

    /* cleanup logfile */
    if(errors == 0) {
        ok(unlink(logfile) == 0, "removed temporary logfile: %s", logfile);
    } else {
        ok(TRUE, "not removed temporary logfile due to errors: %s", logfile);
    }

    gm_free(line);
    return;
}

/* diag queues */
void diag_queues(void);
void diag_queues() {
    char * message = NULL;
    char * version = NULL;
    int rc, x;
    mod_gm_server_status_t *stats;

    // print some debug info
    stats = malloc(sizeof(mod_gm_server_status_t));
    stats->function_num = 0;
    stats->worker_num   = 0;
    rc = get_gearman_server_data(stats, &message, &version, "127.0.0.1", GEARMAND_TEST_PORT);
    diag("get_gearman_server_data:  rc: %d\n", rc);
    diag("get_gearman_server_data: msg: %s\n", message);
    diag("get_gearman_server_data: ver: %s\n", version);
    if( rc == STATE_OK ) {
        diag("%-35s %-9s %-9s\n", "queue", "waiting", "running");
        for(x=0; x<stats->function_num;x++) {
            diag("%-35s %-9d %-9d\n", stats->function[x].queue, stats->function[x].waiting, stats->function[x].running);
        }
    }
    gm_free(message);
    gm_free(version);
    free_mod_gm_status_server(stats);
    return;
}

/* wait till the given queue is empty */
void wait_for_empty_queue(char *queue, int timeout);
void wait_for_empty_queue(char *queue, int timeout) {
    char * message = NULL;
    char * version = NULL;
    int rc, x;
    mod_gm_server_status_t *stats;

    int tries = 0;
    int found = 0;
    while(tries <= timeout*10 && found == 0) {
        tries++;
        stats = malloc(sizeof(mod_gm_server_status_t));
        stats->function_num = 0;
        stats->worker_num   = 0;
        rc = get_gearman_server_data(stats, &message, &version, "127.0.0.1", GEARMAND_TEST_PORT);
        if( rc == STATE_OK ) {
            for(x=0; x<stats->function_num;x++) {
                if(stats->function[x].waiting == 0 &&
                   stats->function[x].running == 0 &&
                   !strcmp( stats->function[x].queue, queue )
                ) {
                    found = 1;
                }
            }
        }
        gm_free(message);
        gm_free(version);
        free_mod_gm_status_server(stats);
        usleep(100000);
    }

    ok(tries < timeout, "queue %s empty after %.1f seconds", queue, (double)tries/10);

    if(tries >= timeout) {
        diag_queues();
    }

    return;
}

char* my_tmpfile(void);
char* my_tmpfile() {
    char *sfn = strdup("/tmp/modgm.XXXXXX");
    int fd = -1;
    if ((fd = mkstemp(sfn)) == -1) {
        fprintf(stderr, "%s: %s\n", sfn, strerror(errno));
        gm_free(sfn);
        return (NULL);
    }
    close(fd);
    return sfn;
}

void check_no_worker_running(char*);
void check_no_worker_running(char* logfile) {
    char cmd[150];
    char *result, *error;
    int rrc;

    // ensure no worker are running anymore
    char *username=getenv("USER");
    snprintf(cmd, 150, "ps -efl 2>/dev/null | grep -v grep | grep '%s' | grep mod_gearman_worker", username);
    rrc = real_exit_code(run_check(cmd, &result, &error));
    ok(rrc == 1, "no worker running anymore");
    like(result, "^\\s*$", "ps output should be empty");
    like(error, "^\\s*$", "ps error output should be empty");
    if(rrc != 1) {
        check_logfile(logfile, 3);
    }
    gm_free(result);
    gm_free(error);
    return;
}


/* main tests */
int main (__attribute__((unused)) int argc, __attribute__((unused)) char **argv, __attribute__((unused)) char **env) {
    int status, chld;
    int tests = 122;
    int rrc;
    char cmd[150];
    char *result, *error;
    plan(tests);

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

#ifdef EMBEDDEDPERL
    char p1[150];
    snprintf(p1, 150, "--p1_file=worker/mod_gearman_p1.pl");
    parse_args_line(mod_gm_opt, p1, 0);
    init_embedded_perl(env);
#endif

    char options[150];
    snprintf(options, 150, "--server=127.0.0.1:%d", GEARMAND_TEST_PORT);
    ok(parse_args_line(mod_gm_opt, options, 0) == 0, "parse_args_line()");
    mod_gm_opt->debug_level = GM_LOG_ERROR;

    worker_logfile = my_tmpfile();
    if(!ok(worker_logfile != NULL, "created temp logile: %s", worker_logfile)) {
        diag("could not create temp logfile");
        exit( EXIT_FAILURE );
    }

    /* first fire up a gearmand server and one worker */
    start_gearmand((void*)NULL);
    sleep(2);
    start_worker((void*)NULL);
    sleep(2);

    /* wait one second and catch died procs */
    while((chld = waitpid(-1, &status, WNOHANG)) != -1 && chld > 0) {
        diag( "waitpid() %d exited with %d\n", chld, status);
        status = 0;
    }

    if(!ok(gearmand_pid > 0, "'gearmand started with port %d and pid: %d", GEARMAND_TEST_PORT, gearmand_pid)) {
        diag("make sure gearmand is in your PATH. Common locations are /usr/sbin or /usr/local/sbin");
        exit( EXIT_FAILURE );
    }
    if(!ok(pid_alive(gearmand_pid) == TRUE, "gearmand alive")) {
        check_logfile("/tmp/gearmand.log", 3);
        kill(gearmand_pid, SIGTERM);
        kill(worker_pid, SIGTERM);
        exit( EXIT_FAILURE );
    }
    if(!ok(worker_pid > 0, "worker started with pid: %d", worker_pid))
        diag("could not start worker");
    if(!ok(pid_alive(worker_pid) == TRUE, "worker alive")) {
        check_logfile(worker_logfile, 3);
        kill(gearmand_pid, SIGTERM);
        kill(worker_pid, SIGTERM);
        exit( EXIT_FAILURE );
    }

    skip(gearmand_pid <= 0 || worker_pid <= 0,
               tests-3,             /* Number of tests to skip */
               "Skipping all tests, no need to go on without gearmand or worker");

    /* create server / clients */
    mod_gm_opt->transportmode = GM_ENCODE_ONLY;
    create_modules();

    /* send big job */
    send_big_jobs(GM_ENCODE_ONLY);
    //diag_queues();
    wait_for_empty_queue("eventhandler", 20);
    wait_for_empty_queue("service", 20);
    //diag_queues();
    do_result_work(1);
    //diag_queues();
    wait_for_empty_queue(GM_DEFAULT_RESULT_QUEUE, 5);

    /*****************************************
     * test check
     */
    //diag_queues();
    test_servicecheck(GM_ENCODE_ONLY, "./t/crit.pl");
    //diag_queues();
    wait_for_empty_queue("eventhandler", 20);
    wait_for_empty_queue("service", 5);
    //diag_queues();
    do_result_work(1);
    //diag_queues();
    wait_for_empty_queue(GM_DEFAULT_RESULT_QUEUE, 5);
    //diag_queues();
    like(last_result, "test plugin CRITICAL", "stdout output from ./t/crit.pl");
    like(last_result, "some errors on stderr", "stderr output from ./t/crit.pl");

    /*****************************************
     * test check2
     */
    //diag_queues();
    test_servicecheck(GM_ENCODE_ONLY, "./t/both");
    //diag_queues();
    wait_for_empty_queue("eventhandler", 20);
    wait_for_empty_queue("service", 5);
    //diag_queues();
    do_result_work(1);
    //diag_queues();
    wait_for_empty_queue(GM_DEFAULT_RESULT_QUEUE, 5);
    like(last_result, "stdout output", "stdout output from ./t/both");
    like(last_result, "stderr output", "stderr output from ./t/both");

    /* try to send some data with base64 only */
    //diag_queues();
    test_eventhandler(GM_ENCODE_ONLY);
    //diag_queues();
    test_servicecheck(GM_ENCODE_ONLY, NULL);
    //diag_queues();
    wait_for_empty_queue("eventhandler", 20);
    wait_for_empty_queue("service", 5);
    //diag_queues();
    do_result_work(1);
    //diag_queues();
    wait_for_empty_queue(GM_DEFAULT_RESULT_QUEUE, 5);
    sleep(1);
    kill(worker_pid, SIGTERM);
    waitpid(worker_pid, &status, 0);
    ok(status == 0, "worker (%d) exited with exit code %d", worker_pid, real_exit_code(status));
    status = 0;
    check_no_worker_running(worker_logfile);
    check_logfile(worker_logfile, 0);

    char * test_keys[] = {
        "12345",
        "test",
        "test key 123",
        "me make you loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong key"
    };

    /* ignore some signals for now */
    signal(SIGTERM, SIG_IGN);

    int i;
    for(i=0;i<4;i++) {
        mod_gm_opt->transportmode = GM_ENCODE_AND_ENCRYPT;
        start_worker((void *)test_keys[i]);

        test_ctx = mod_gm_crypt_init( test_keys[i] );
        ok(1, "initialized with key: %s", test_keys[i]);

        test_eventhandler(GM_ENCODE_AND_ENCRYPT);
        test_servicecheck(GM_ENCODE_AND_ENCRYPT, NULL);
        wait_for_empty_queue("eventhandler", 20);
        wait_for_empty_queue("service", 5);
        do_result_work(1);
        wait_for_empty_queue(GM_DEFAULT_RESULT_QUEUE, 5);
        sleep(1);

        kill(worker_pid, SIGTERM);
        waitpid(worker_pid, &status, 0);
        ok(status == 0, "worker (%d) exited with exit code %d", worker_pid, real_exit_code(status));
        status = 0;
        check_no_worker_running(worker_logfile);
        check_logfile(worker_logfile, 0);
        mod_gm_crypt_deinit(test_ctx);
    }

    /*****************************************
     * send_gearman
     */
    snprintf(cmd, 150, "./send_gearman --server=127.0.0.1:%d --key=testtest --host=test --service=test --message=test --returncode=0", GEARMAND_TEST_PORT);
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "^\\s*$", "output from ./send_gearman");
    gm_free(result);
    gm_free(error);

    /*****************************************
     * send_multi
     */
    snprintf(cmd, 150, "./send_multi --server=127.0.0.1:%d --host=blah < t/data/send_multi.txt", GEARMAND_TEST_PORT);
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "send_multi OK: 2 check_multi child checks submitted", "output from ./send_multi");
    gm_free(result);
    gm_free(error);

    /*****************************************
     * check_gearman
     */
    snprintf(cmd, 150, "./check_gearman -H 127.0.0.1:%d -s check -a -q worker_test", GEARMAND_TEST_PORT);
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "check_gearman OK - sending background job succeded", "output from ./check_gearman");

    /* cleanup */
    gm_free(result);
    gm_free(error);
    gm_free_client(&client);
    gm_free_worker(&worker);

    /* shutdown gearmand */
    kill(gearmand_pid, SIGTERM);
    waitpid(gearmand_pid, &status, 0);
    ok(status == 0, "gearmand (%d) exited with exit code %d", gearmand_pid, real_exit_code(status));
    status = 0;

    todo();
    check_logfile("/tmp/gearmand.log", status != 0 ? 2 : 0);
    end_todo;
    status = 0;

    kill(worker_pid, SIGTERM);
    waitpid(worker_pid, &status, 0);
    ok(status == 0, "worker (%d) exited with exit code %d", worker_pid, real_exit_code(status));
    check_no_worker_running(worker_logfile);
    status = 0;

#ifdef EMBEDDEDPERL
    deinit_embedded_perl(0);
#endif

    gm_free(last_result);
    gm_free(worker_logfile);
    end_skip;
    mod_gm_free_opt(mod_gm_opt);
    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
