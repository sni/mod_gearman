#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <t/tap.h>
#include <worker_logger.h>
#include <common.h>
#include <utils.h>
#include <gearman.h>

#define GEARMAND_TEST_PORT   54730

int gearmand_pid;
int worker_pid;
gearman_worker_st worker;
gearman_client_st client;
mod_gm_opt_t *mod_gm_opt;


/* start the gearmand server */
void *start_gearmand(void*data) {
    int sid;
    data = data; // warning: unused parameter 'data'
    pid_t pid = fork();
    if(pid == 0) {
        sid = setsid();
        char port[30];
        snprintf(port, 30, "-p %d", GEARMAND_TEST_PORT);
        execlp("gearmand", "-t 10", "-j 0", port, (char *)NULL);
        perror("gearmand");
        exit(1);
    }
    else {
        gearmand_pid = pid;
    }
    return NULL;
}


/* start the test worker */
void *start_worker(void*data) {
    int sid;
    char* key = (char*)data;
    pid_t pid = fork();
    if(pid == 0) {
        sid = setsid();
        char options[150];
        snprintf(options, 150, "server=localhost:%d", GEARMAND_TEST_PORT);
        if(key != NULL) {
            char encryption[150];
            snprintf(encryption, 150, "key=%s", key);
            execl("./mod_gearman_worker", "debug=0", encryption,      "logfile=/dev/null", "max-worker=1", options, (char *)NULL);
        } else {
            execl("./mod_gearman_worker", "debug=0", "encryption=no", "logfile=/dev/null", "max-worker=1", options, (char *)NULL);
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
void test_eventhandler(int transportmode) {
    char * testdata = "type=eventhandler\ncommand_line=/bin/hostname\n\n\n";
    int rt = add_job_to_queue( &client, mod_gm_opt->server_list, "eventhandler", NULL, testdata, GM_JOB_PRIO_NORMAL, 1, transportmode);
    ok(rt == GM_OK, "eventhandler sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    return;
}


/* test service check handler over gearmand */
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
    int rt = add_job_to_queue( &client, mod_gm_opt->server_list, "service", NULL, temp_buffer, GM_JOB_PRIO_NORMAL, 1, transportmode);
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


/* main tests */
int main(void) {
    int status;
    int tests = 25;
    plan_tests(tests);

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    char options[150];
    snprintf(options, 150, "--server=localhost:%d", GEARMAND_TEST_PORT);
    ok(parse_args_line(mod_gm_opt, options, 0) == 0, "parse_args_line()");
    mod_gm_opt->debug_level = GM_LOG_ERROR;

    /* first fire up a gearmand server and one worker */
    start_gearmand((void*)NULL);
    start_worker((void*)NULL);

    sleep(1);
    if(!ok(gearmand_pid > 0, "gearmand running with pid: %d", gearmand_pid))
        diag("make sure gearmand is in your PATH. Usuall locations are /usr/sbin or /usr/local/sbin");
    if(!ok(worker_pid > 0, "worker running with pid: %d", worker_pid))
        diag("could not start worker");

    skip_start(gearmand_pid <= 0 || worker_pid <= 0,
               tests-3,             /* Number of tests to skip */
               "Skipping all tests, no need to go on without gearmand or worker");

    /* create server / worker / clients */
    create_modules();

    /* try to send some data with base64 only */
    test_eventhandler(GM_ENCODE_ONLY);
    test_servicecheck(GM_ENCODE_ONLY);

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
        start_worker((void *)test_keys[i]);
        sleep(1);

        mod_gm_crypt_init( test_keys[i] );
        ok(1, "initialized with key: %s", test_keys[i]);

        test_eventhandler(GM_ENCODE_AND_ENCRYPT);
        test_servicecheck(GM_ENCODE_AND_ENCRYPT);
    }

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

    skip_end;
    return exit_status();
}
