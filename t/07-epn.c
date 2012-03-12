#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <t/tap.h>
#include <config.h>
#include <common.h>
#include <utils.h>
#include <check_utils.h>
#ifdef EMBEDDEDPERL
#include <epn_utils.h>
#endif

mod_gm_opt_t *mod_gm_opt;

#ifdef EMBEDDEDPERL
extern char* p1_file;
#endif

int main (int argc, char **argv, char **env) {
    argc = argc; argv = argv; env  = env;
#ifndef EMBEDDEDPERL
    plan(1);
    ok(1, "skipped epn tests");
    return exit_status();
#endif
#ifdef EMBEDDEDPERL
    int rc, rrc;
    char *result, *error;
    char cmd[120];

    plan(17);

    /* create options structure and set debug level */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    char cmds[150];
    strcpy(cmds, "--p1_file=worker/mod_gearman_p1.pl");
    parse_args_line(mod_gm_opt, cmds, 0);
    strcpy(cmds, "--enable_embedded_perl=on");
    parse_args_line(mod_gm_opt, cmds, 0);
    strcpy(cmds, "--use_embedded_perl_implicitly=on");
    parse_args_line(mod_gm_opt, cmds, 0);
    /*
     * mod_gm_opt->debug_level=4;
     * dumpconfig(mod_gm_opt, GM_WORKER_MODE);
     */
    ok(p1_file != NULL, "p1_file: %s", p1_file);

    /*****************************************
     * send_gearman
     */
    init_embedded_perl(env);

    rc=file_uses_embedded_perl("t/ok.pl");
    cmp_ok(rc, "==", TRUE, "ok.pl: file_uses_embedded_perl returned rc %d", rc);

    rc=file_uses_embedded_perl("t/noepn.pl");
    cmp_ok(rc, "==", FALSE, "noepn.pl: file_uses_embedded_perl returned rc %d", rc);

    strcpy(cmd, "./t/fail.pl");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 3, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "ePN failed to compile", "returned result string");
    like(error, "^$", "returned error string");
    free(result);
    free(error);

    strcpy(cmd, "./t/ok.pl");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "test plugin OK", "returned result string");
    unlike(result, "plugin did not call exit", "returned result string");
    like(error, "^$", "returned error string");
    free(result);
    free(error);

    strcpy(cmd, "./t/crit.pl");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 2, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "test plugin CRITICAL", "returned result string");
    like(error, "some errors on stderr", "returned error string");
    free(result);
    free(error);

    strcpy(cmd, "./t/noexit.pl");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 3, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "sample output but no exit", "returned result string");
    like(result, "plugin did not call exit", "returned result string");
    like(error, "^$", "returned error string");
    free(result);
    free(error);

    /*****************************************
     * clean up
     */
    mod_gm_free_opt(mod_gm_opt);
    deinit_embedded_perl();

    return exit_status();
#endif
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
