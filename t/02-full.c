#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>

#include <t/tap.h>
#include <worker_logger.h>
#include <common.h>
#include <utils.h>
#include <gearman.h>

#define GEARMAND_TEST_PORT   54730

int gearmand_pid;
gearman_worker_st worker;
gearman_client_st client;
char * server_list[1];


/* start the gearmand server */
void *start_gearmand(void*data) {
    data = data; // warning: unused parameter 'data'
    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );
    pid_t pid = fork();
    if(pid == 0) {
        char port[30];
        snprintf(port, 30, "-p %d", GEARMAND_TEST_PORT);
        execlp("gearmand", "-t 10", "-j 0", port, (char *)NULL);
        perror("gearmand");
        return NULL;
    }
    else {
        gearmand_pid = pid;
        int status;
        waitpid(pid, &status, 0);
    }
    return NULL;
}


/* test event handler over gearmand */
void test_eventhandler(int transportmode) {
    char * testdata = "type=eventhandler\ncommand_line=/bin/hostname\n\n\n";
    int rt = add_job_to_queue( &client, server_list, "eventhandler", NULL, testdata, GM_JOB_PRIO_NORMAL, 1, transportmode);
    ok(rt == GM_OK, "eventhandler sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
    return;
}


/* create server / worker / clients */
void create_modules(void);
void create_modules() {
    char * server_list[1];
    server_list[0] = malloc(30);
    snprintf(server_list[0], 30, "%s:%d", "localhost", GEARMAND_TEST_PORT);
    ok(create_client( server_list, &client ) == GM_OK, "created test client");
    ok(create_worker( server_list, &worker ) == GM_OK, "created test worker");
    return;
}


/* main tests */
int main(void) {
    int tests = 15;
    plan_tests(tests);


    /* first fire up a gearmand server */
    pthread_t gearmand_thread;
    pthread_create( &gearmand_thread, NULL, start_gearmand, (void *)NULL);
    sleep(1);
    if(!ok(gearmand_pid > 0, "gearmand running with pid: %d", gearmand_pid))
        diag("make sure gearmand is in your PATH. Usuall locations are /usr/sbin or /usr/local/sbin");

    skip_start(gearmand_pid <= 0,   /* Boolean expression      */
               tests-1,             /* Number of tests to skip */
               "Skipping all tests, no need to go on without gearmand");

    /* create server / worker / clients */
    create_modules();

    char * test_keys[] = {
        "",
        "test",
        "test key 123",
        "me make you loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong key"
    };

    int i;
    for(i=0;i<4;i++) {
        mod_gm_crypt_init( test_keys[i] );
        ok(1, "initialized with key: %s", test_keys[i]);

        /* try to send some data */
        test_eventhandler(GM_ENCODE_ONLY);
        test_eventhandler(GM_ENCODE_AND_ENCRYPT);
    }

    /* cleanup */
    kill(gearmand_pid, SIGTERM);
    skip_end;
    pthread_cancel(gearmand_thread);
    pthread_join(gearmand_thread, NULL);
    return exit_status();
}
