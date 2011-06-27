#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <t/tap.h>
#include <common.h>
#include <utils.h>
#include <gearman.h>

#define GEARMAND_TEST_PORT   54730

char * worker_logfile;
int gearmand_pid;
int worker_pid;
gearman_worker_st worker;
gearman_client_st client;
mod_gm_opt_t *mod_gm_opt;


/* start the gearmand server */
void *start_gearmand(void*data);
void *start_gearmand(void*data) {
    int sid;
    data = data; // warning: unused parameter 'data'
    pid_t pid = fork();
    if(pid == 0) {
        sid = setsid();
        char port[30];
        snprintf(port, 30, "--port=%d", GEARMAND_TEST_PORT);
        /* for newer gearman versions */
        if(atof(gearman_version()) > 0.14) {
            execlp("gearmand", "gearmand", "--threads=10", "--job-retries=0", port, "--verbose=5", (char *)NULL);
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
    int sid;
    char* key = (char*)data;
    pid_t pid = fork();
    if(pid == 0) {
        sid = setsid();
        char options[150];
        snprintf(options, 150, "server=localhost:%d", GEARMAND_TEST_PORT);
        char logf[150];
        snprintf(logf, 150, "logfile=%s", worker_logfile);
        if(key != NULL) {
            char encryption[150];
            snprintf(encryption, 150, "key=%s", key);
            execl("./mod_gearman_worker", "./mod_gearman_worker", "debug=2", encryption,      logf, "max-worker=1", options, (char *)NULL);
        } else {
            execl("./mod_gearman_worker", "./mod_gearman_worker", "debug=2", "encryption=no", logf, "max-worker=1", options, (char *)NULL);
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
    char * testdata = "type=eventhandler\ncommand_line=/bin/hostname\n\n\n";
    int rt = add_job_to_queue( &client, mod_gm_opt->server_list, "eventhandler", NULL, testdata, GM_JOB_PRIO_NORMAL, 1, transportmode, TRUE );
    ok(rt == GM_OK, "eventhandler sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    return;
}


/* test service check handler over gearmand */
void test_servicecheck(int transportmode);
void test_servicecheck(int transportmode) {
    struct timeval start_time;
    gettimeofday(&start_time,NULL);
    char temp_buffer[GM_BUFFERSIZE];
    temp_buffer[0]='\x0';
    snprintf( temp_buffer,sizeof( temp_buffer )-1,"type=service\nresult_queue=%s\nhost_name=%s\nservice_description=%s\nstart_time=%i.%i\ntimeout=%d\ncheck_options=%i\nscheduled_check=%i\nreschedule_check=%i\nlatency=%f\ncommand_line=%s\n\n\n",
              "result_queue",
              "host1",
              "service1",
              ( int )start_time.tv_sec,
              ( int )start_time.tv_usec,
              60,
              0,
              1,
              1,
              0.0,
              "/bin/hostname"
            );
    temp_buffer[sizeof( temp_buffer )-1]='\x0';
    int rt = add_job_to_queue( &client, mod_gm_opt->server_list, "service", NULL, temp_buffer, GM_JOB_PRIO_NORMAL, 1, transportmode, TRUE );
    ok(rt == GM_OK, "servicecheck sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    return;
}


/* create server / worker / clients */
void create_modules(void);
void create_modules() {
    ok(create_client( mod_gm_opt->server_list, &client ) == GM_OK, "created test client");
    ok(create_worker( mod_gm_opt->server_list, &worker ) == GM_OK, "created test worker");
    return;
}


/* check logfile for errors */
void check_logfile(void);
void check_logfile() {
    FILE * fp;
    char *line;
    int errors = 0;

    fp = fopen(worker_logfile, "r");
    if(fp == NULL) {
        perror(worker_logfile);
        return;
    }
    line = malloc(GM_BUFFERSIZE);
    while(fgets(line, GM_BUFFERSIZE, fp) != NULL) {
        trim(line);
        if(strstr(line, "ERROR") != NULL) {
            errors++;
            diag("logfile: %s", line);
        }
    }
    fclose(fp);

    ok(errors == 0, "errors in logfile: %d", errors);

    /* cleanup logfile */
    ok(unlink(worker_logfile) == 0, "removed temporary logfile: %s", worker_logfile);

    free(line);
    return;
}


/* main tests */
int main(void) {
    int status, chld;
    int tests = 40;
    int rrc;
    char cmd[150];
    char * result;
    plan(tests);

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    char options[150];
    snprintf(options, 150, "--server=localhost:%d", GEARMAND_TEST_PORT);
    ok(parse_args_line(mod_gm_opt, options, 0) == 0, "parse_args_line()");
    mod_gm_opt->debug_level = GM_LOG_ERROR;

    worker_logfile = tmpnam(NULL);
    if(!ok(worker_logfile != NULL, "created temp logile: %s", worker_logfile)) {
        diag("could not create temp logfile");
        exit( EXIT_FAILURE );
    }

    /* first fire up a gearmand server and one worker */
    start_gearmand((void*)NULL);
    start_worker((void*)NULL);

    /* wait one second and catch died procs */
    sleep(1);
    while((chld = waitpid(-1, &status, WNOHANG)) != -1 && chld > 0) {
        diag( "waitpid() %d exited with %d\n", chld, status);
    }

    if(!ok(gearmand_pid > 0, "'gearmand --threads=10 --job-retries=0 --port=%d --verbose=5' started with pid: %d", GEARMAND_TEST_PORT, gearmand_pid)) {
        diag("make sure gearmand is in your PATH. Common locations are /usr/sbin or /usr/local/sbin");
        exit( EXIT_FAILURE );
    }
    if(!ok(pid_alive(gearmand_pid) == TRUE, "gearmand alive")) {
        exit( EXIT_FAILURE );
    }
    if(!ok(worker_pid > 0, "worker running with pid: %d", worker_pid))
        diag("could not start worker");
    if(!ok(pid_alive(worker_pid) == TRUE, "worker alive")) {
        exit( EXIT_FAILURE );
    }

    skip(gearmand_pid <= 0 || worker_pid <= 0,
               tests-3,             /* Number of tests to skip */
               "Skipping all tests, no need to go on without gearmand or worker");
    sleep(2);

    /* create server / worker / clients */
    create_modules();

    /* try to send some data with base64 only */
    test_eventhandler(GM_ENCODE_ONLY);
    test_servicecheck(GM_ENCODE_ONLY);
    sleep(1);

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
        kill(worker_pid, SIGTERM);
        waitpid(worker_pid, &status, 0);
        ok(status == 0, "worker exited with exit code %d", real_exit_code(status));
        check_logfile();
        start_worker((void *)test_keys[i]);
        sleep(1);

        mod_gm_crypt_init( test_keys[i] );
        ok(1, "initialized with key: %s", test_keys[i]);

        test_eventhandler(GM_ENCODE_AND_ENCRYPT);
        test_servicecheck(GM_ENCODE_AND_ENCRYPT);
        sleep(1);
    }

    /*****************************************
     * send_gearman
     */
    snprintf(cmd, 150, "./send_gearman --server=localhost:%d --key=testtest --host=test --service=test --message=test --returncode=0", GEARMAND_TEST_PORT);
    rrc = real_exit_code(run_check(cmd, &result));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "^\s*$", "output from ./send_gearman");

    /*****************************************
     * send_gearman
     */
    snprintf(cmd, 150, "./send_multi --server=localhost:%d --host=blah < t/data/send_multi.txt", GEARMAND_TEST_PORT);
    rrc = real_exit_code(run_check(cmd, &result));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "send_multi OK: 2 check_multi child checks submitted", "output from ./send_multi");


    /* cleanup */
    mod_gm_free_opt(mod_gm_opt);
    free_client(&client);
    free_worker(&worker);

    kill(gearmand_pid, SIGTERM);
    waitpid(gearmand_pid, &status, 0);
    ok(status == 0, "gearmand exited with exit code %d", real_exit_code(status));

    kill(worker_pid, SIGTERM);
    waitpid(worker_pid, &status, 0);
    ok(status == 0, "worker exited with exit code %d", real_exit_code(status));

    endskip;
    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
