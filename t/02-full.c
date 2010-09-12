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
mod_gm_opt_t *mod_gm_opt;


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
    int rt = add_job_to_queue( &client, mod_gm_opt->server_list, "eventhandler", NULL, testdata, GM_JOB_PRIO_NORMAL, 1, transportmode);
    ok(rt == GM_OK, "eventhandler sent successfully in mode %s", transportmode == GM_ENCODE_ONLY ? "base64" : "aes256");
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
    int tests = 16;
    plan_tests(tests);

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    char options[150];
    snprintf(options, 150, "--server=localhost:%d", GEARMAND_TEST_PORT);
    ok(parse_args_line(mod_gm_opt, options, 0) == 0, "parse_args_line()");
    mod_gm_opt->debug_level = GM_LOG_ERROR;

    /* first fire up a gearmand server */
    pthread_t gearmand_thread;
    pthread_create( &gearmand_thread, NULL, start_gearmand, (void *)NULL);
    sleep(1);
    if(!ok(gearmand_pid > 0, "gearmand running with pid: %d", gearmand_pid))
        diag("make sure gearmand is in your PATH. Usuall locations are /usr/sbin or /usr/local/sbin");

    skip_start(gearmand_pid <= 0,   /* Boolean expression      */
               tests-2,             /* Number of tests to skip */
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
    mod_gm_free_opt(mod_gm_opt);
    free_client(&client);
    free_worker(&worker);
    kill(gearmand_pid, SIGTERM);
    skip_end;
    pthread_cancel(gearmand_thread);
    pthread_join(gearmand_thread, NULL);
    return exit_status();
}
