#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common.h>
#include <utils.h>
#include <check_utils.h>
#ifdef EMBEDDEDPERL
#include <epn_utils.h>
#endif

mod_gm_opt_t *mod_gm_opt;

int main (int argc, char **argv, char **env) {
    argc = argc; argv = argv; env  = env;
    char *result, *error;
    char cmd[120];
    int x;

    /* set hostname */
    gethostname(hostname, GM_BUFFERSIZE-1);

    /* create options structure and set debug level */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    mod_gm_opt->debug_level = 4;

#ifdef EMBEDDEDPERL
    char p1[150];
    snprintf(p1, 150, "--p1_file=worker/mod_gearman_p1.pl");
    parse_args_line(mod_gm_opt, p1, 0);
    init_embedded_perl(env);
#endif

    gm_job_t * exec_job;
    exec_job = ( gm_job_t * )malloc( sizeof *exec_job );
    set_default_job(exec_job, mod_gm_opt);
    strcpy(cmd, "BLAH=BLUB /bin/hostname");
    printf("this should be popen\n");
    run_check(cmd, &result, &error);
    free(result);
    free(error);

    strcpy(cmd, "/bin/hostname");
    printf("this should be execvp\n");
    run_check(cmd, &result, &error);
    free(result);
    free(error);
    mod_gm_opt->debug_level = 0;

    for(x=0;x<100;x++) {
        run_check(cmd, &result, &error);
        free(result);
        free(error);
    }


    free_job(exec_job);
    mod_gm_free_opt(mod_gm_opt);
#ifdef EMBEDDEDPERL
    deinit_embedded_perl(0);
#endif
    exit(0);
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
