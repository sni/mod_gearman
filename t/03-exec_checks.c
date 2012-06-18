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
#include "gearman_utils.h"

mod_gm_opt_t *mod_gm_opt;

int main (int argc, char **argv, char **env) {
    argc = argc; argv = argv; env  = env;
    int rc, rrc;
    char *result, *error;
    char cmd[120];
    char hostname[GM_BUFFERSIZE];

    plan(60);

    /* set hostname */
    gethostname(hostname, GM_BUFFERSIZE-1);

    /* create options structure and set debug level */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    mod_gm_opt->debug_level = 0;

#ifdef EMBEDDEDPERL
    char p1[150];
    snprintf(p1, 150, "--p1_file=worker/mod_gearman_p1.pl");
    parse_args_line(mod_gm_opt, p1, 0);
    init_embedded_perl(env);
#endif

    /*****************************************
     * arg parsing test 1
     */
    char *args[MAX_CMD_ARGS];
    strcpy(cmd, "/bin/hostname");
    parse_command_line(cmd, args);
    like(args[0], cmd, "parsing args cmd 1");

    /*****************************************
     * arg parsing test 2
     */
    strcpy(cmd, "/bin/cmd blah blub   foo");
    parse_command_line(cmd,args);
    like(args[0], "/bin/cmd", "parsing args cmd 2");
    like(args[1], "blah", "parsing args cmd 2");
    like(args[2], "blub", "parsing args cmd 2");
    like(args[3], "foo", "parsing args cmd 2");

    /*****************************************
     * send_gearman 1
     */
    strcpy(cmd, "./send_gearman --server=blah --key=testtest --host=test --service=test --message=test --returncode=0");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 3, "cmd '%s' returned rc %d", cmd, rrc);
    if(atof(gearman_version()) >= 0.31) {
        like(result, "send_gearman UNKNOWN:", "result");
    } else {
        like(result, "sending job to gearmand failed:", "result");
    }
    free(result);
    free(error);

    /*****************************************
     * send_gearman 2
     */
    strcpy(cmd, "./send_gearman --server=blah < t/data/send_gearman_results.txt");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 3, "cmd '%s' returned rc %d", cmd, rrc);
    if(atof(gearman_version()) >= 0.31) {
        like(result, "send_gearman UNKNOWN:", "result");
    } else {
        like(result, "sending job to gearmand failed:", "result");
    }
    free(result);
    free(error);

    /*****************************************
     * send_multi
     */
    strcpy(cmd, "./send_multi --server=blah --host=blah < t/data/send_multi.txt");
    rrc = real_exit_code(run_check(cmd, &result, &error));
    cmp_ok(rrc, "==", 3, "cmd '%s' returned rc %d", cmd, rrc);
    if(atof(gearman_version()) >= 0.31) {
        like(result, "send_multi UNKNOWN:", "result");
    } else {
        like(result, "sending job to gearmand failed:", "result");
    }
    free(result);
    free(error);

    /*****************************************
     * simple test command 1
     */
    strcpy(cmd, "/bin/hostname");
    rc = run_check(cmd, &result, &error);
    cmp_ok(rc, "==", 0, "pclose for cmd '%s' returned rc %d", cmd, rc);
    rrc = real_exit_code(rc);
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    free(result);
    free(error);

    /*****************************************
     * simple test command 2
     */
    strcpy(cmd, "/bin/hostname 2>&1");
    rc = run_check(cmd, &result, &error);
    cmp_ok(rc, "==", 0, "pclose for cmd '%s' returned rc %d", cmd, rc);
    rrc = real_exit_code(rc);
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    free(result);
    free(error);

    /*****************************************
     * simple test command 3
     */
    strcpy(cmd, "/usr/lib/nagios/plugins/check_icmp -H 127.0.0.1");
    rc = run_check(cmd, &result, &error);
    cmp_ok(rc, "==", 0, "pclose for cmd '%s' returned rc %d", cmd, rc);
    rrc = real_exit_code(rc);
    cmp_ok(rrc, "==", 0, "cmd '%s' returned rc %d", cmd, rrc);
    free(result);
    free(error);

    /*****************************************
     * simple test command 4
     */
    strcpy(cmd, "echo -n 'test'; exit 2");
    rc  = run_check(cmd, &result, &error);
    rrc = real_exit_code(rc);
    cmp_ok(rrc, "==", 2, "cmd '%s' returned rc %d", cmd, rrc);
    like(result, "test", "returned result string");
    free(result);
    free(error);

    gm_job_t * exec_job;
    exec_job = ( gm_job_t * )malloc( sizeof *exec_job );
    set_default_job(exec_job, mod_gm_opt);



    /*****************************************
     * non existing command 1
     */
    exec_job->command_line = strdup("/bin/doesntexist");
    exec_job->type         = strdup("service");
    exec_job->timeout      = 10;
    int fork_on_exec       = 0;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 127 is out of bounds. Make sure the plugin you're trying to run actually exists. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    fork_on_exec = 1;
    lives_ok({execute_safe_command(exec_job, fork_on_exec, hostname);}, "executing command using fork on exec");
    free(exec_job->output);
    free(exec_job->error);

    /* non existing command 2 */
    free(exec_job->command_line);
    exec_job->command_line = strdup("/bin/doesntexist 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 127 is out of bounds. Make sure the plugin you're trying to run actually exists. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);



    /*****************************************
     * non executable command 1
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./THANKS");
    fork_on_exec           = 0;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* non existing command 2 */
    fork_on_exec = 1;
    free(exec_job->command_line);
    exec_job->command_line = strdup("./THANKS 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);



    /*****************************************
     * unknown exit code 1
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/rc 5");

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 5 is out of bounds. \\(worker:.*exiting with exit code 5", "returned result string");
    free(exec_job->output);
    free(exec_job->error);


    /* unknown exit code 2 */
    fork_on_exec = 0;
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/rc 5 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 5 is out of bounds. \\(worker:.*exiting with exit code 5", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* unknown exit code 3 */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/rc 128 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 128 is out of bounds. Plugin exited by signal signal 0. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* unknown exit code 4 */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/rc 137 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 137 is out of bounds. Plugin exited by signal SIGKILL. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* unknown exit code 5 */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/rc 255 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 255 is out of bounds. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);


    /*****************************************
     * signaled exit code SIGINT
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/killer INT");
    fork_on_exec           = 1;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 130 is out of bounds. Plugin exited by signal SIGINT. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* signaled exit code SIGINT 2 */
    fork_on_exec = 0;
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/killer INT 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 130 is out of bounds. Plugin exited by signal SIGINT. \\(worker:", "returned result string");
    free(exec_job->output);
    free(exec_job->error);



    /*****************************************
     * timed out check
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/sleep 30");
    exec_job->timeout      = 1;
    fork_on_exec           = 1;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "\\(Service Check Timed Out On Worker: ", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* timed out check 2 */
    fork_on_exec = 0;
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/sleep 30 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "\\(Service Check Timed Out On Worker: ", "returned result string");
    free(exec_job->output);
    free(exec_job->error);

    /* reset timeout */
    exec_job->timeout      = 30;



    /*****************************************
     * capture stderr
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/both");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 0, "cmd '%s' returns rc 0", exec_job->command_line);
    like(exec_job->output, "out", "returned result string");
    like(exec_job->error,  "err", "returned error string");
    free(exec_job->output);
    free(exec_job->error);

    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/both;");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 0, "cmd '%s' returns rc 0", exec_job->command_line);
    like(exec_job->output, "out", "returned result string");
    like(exec_job->error,  "err", "returned error string");
    free(exec_job->output);
    free(exec_job->error);

    fork_on_exec = 1;
    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/both");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 0, "cmd '%s' returns rc 0", exec_job->command_line);
    like(exec_job->output, "out", "returned result string");
    like(exec_job->error,  "err", "returned error string");
    free(exec_job->output);
    free(exec_job->error);

    free(exec_job->command_line);
    exec_job->command_line = strdup("./t/both;");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 0, "cmd '%s' returns rc 0", exec_job->command_line);
    like(exec_job->output, "out", "returned result string");
    like(exec_job->error,  "err", "returned error string");
    free(exec_job->output);
    free(exec_job->error);

    /*****************************************
     * cmd env
     */
    free(exec_job->command_line);
    exec_job->command_line = strdup("BLAH=BLUB ./t/ok.pl");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 0, "cmd '%s' returns rc 0", exec_job->command_line);
    like(exec_job->output, "test plugin OK", "returned result string");

    /*****************************************
     * clean up
     */
    free_job(exec_job);
    mod_gm_free_opt(mod_gm_opt);
#ifdef EMBEDDEDPERL
    deinit_embedded_perl(0);
#endif
    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
