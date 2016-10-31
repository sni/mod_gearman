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

#include <worker_dummy_functions.c>

mod_gm_opt_t *mod_gm_opt;

/* main tests */
int main(void) {
    int tests = 4;
    plan(tests);

    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);

    mod_gm_opt->logmode = GM_LOG_MODE_AUTO;
    lives_ok({gm_log(GM_LOG_INFO, "info message auto\n");}, "info message in auto mode");

    mod_gm_opt->logmode = GM_LOG_MODE_STDOUT;
    lives_ok({gm_log(GM_LOG_INFO, "info message stdout\n");}, "info message in stdout mode");

    mod_gm_opt->logmode = GM_LOG_MODE_SYSLOG;
    lives_ok({gm_log(GM_LOG_DEBUG, "info message syslog\n");}, "info message in syslog mode");

    mod_gm_opt->logmode = GM_LOG_MODE_CORE;
    lives_ok({gm_log(GM_LOG_INFO, "info message core\n");}, "info message in core mode");

    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
