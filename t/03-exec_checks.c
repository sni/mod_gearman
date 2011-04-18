#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <t/tap.h>
#include <common.h>
#include <utils.h>

mod_gm_opt_t *mod_gm_opt;

int main(void) {
    int rc, rrc;
    char * result;
    char cmd[100];
    char hostname[GM_BUFFERSIZE];

    plan(40);

    /* set hostname */
    gethostname(hostname, GM_BUFFERSIZE-1);

    /* create options structure and set debug level */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    mod_gm_opt->debug_level = 0;

    /*****************************************
     * arg parsing test 1
     */
    char *argv[MAX_CMD_ARGS];
    strcpy(cmd, "/bin/true");
    parse_command_line(cmd, argv);
    if(!ok(argv[0] == cmd, "parsing args cmd 1"))
        diag("expected '%s' but got '%s'", cmd, argv[0]);

    /*****************************************
     * arg parsing test 2
     */
    strcpy(cmd, "/bin/cmd blah blub   foo");
    parse_command_line(cmd,argv);
    if(!ok(!strcmp(argv[0], "/bin/cmd"), "parsing args cmd 2"))
        diag("expected '/bin/cmd' but got '%s'", argv[0]);
    if(!ok(!strcmp(argv[1], "blah"), "parsing args cmd 2"))
        diag("expected 'blah' but got '%s'", argv[1]);
    if(!ok(!strcmp(argv[2], "blub"), "parsing args cmd 2"))
        diag("expected 'blub' but got '%s'", argv[2]);
    if(!ok(!strcmp(argv[3], "foo"), "parsing args cmd 2"))
        diag("expected 'foo' but got '%s'", argv[3]);

    /*****************************************
     * simple test command 1
     */
    strcpy(cmd, "/bin/true");
    rc = run_check(cmd, &result);
    if(!ok(rc == 0, "pclose for cmd '%s' returned rc %d", cmd, rc))
        diag("cmd: '%s' returned %d", cmd, rc);
    rrc = real_exit_code(rc);
    if(!ok(rrc == 0, "cmd '%s' returned rc %d", cmd, rrc))
        diag("cmd: '%s' returned %d", cmd, rrc);

    /*****************************************
     * simple test command 2
     */
    strcpy(cmd, "/bin/true 2>&1");
    rc = run_check(cmd, &result);
    if(!ok(rc == 0, "pclose for cmd '%s' returned rc %d", cmd, rc))
        diag("cmd: '%s' returned %d", cmd, rc);
    rrc = real_exit_code(rc);
    if(!ok(rrc == 0, "cmd '%s' returned rc %d", cmd, rrc))
        diag("cmd: '%s' returned %d", cmd, rrc);

    /*****************************************
     * simple test command 3
     */
    strcpy(cmd, "/usr/lib/nagios/plugins/check_icmp -H 127.0.0.1");
    rc = run_check(cmd, &result);
    if(!ok(rc == 0, "pclose for cmd '%s' returned rc %d", cmd, rc))
        diag("cmd: '%s' returned %d", cmd, rc);
    rrc = real_exit_code(rc);
    if(!ok(rrc == 0, "cmd '%s' returned rc %d", cmd, rrc))
        diag("cmd: '%s' returned %d", cmd, rrc);

    /*****************************************
     * simple test command 4
     */
    strcpy(cmd, "echo -n 'test'; exit 2");
    rc  = run_check(cmd, &result);
    rrc = real_exit_code(rc);
    if(!ok(rrc == 2, "cmd '%s' returned rc %d", cmd, rrc))
        diag("cmd: '%s' returned %d", cmd, rrc);
    if(!ok(!strcmp(result, "test"), "returned result string"))
        diag("expected 'test' but got '%s'", result);

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

    fork_on_exec = 1;
    lives_ok({execute_safe_command(exec_job, fork_on_exec, hostname);}, "executing command using fork on exec");

    /* non existing command 2 */
    exec_job->command_line = strdup("/bin/doesntexist 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 127 is out of bounds. Make sure the plugin you're trying to run actually exists. \\(worker:", "returned result string");



    /*****************************************
     * non executable command 1
     */
    exec_job->command_line = strdup("./THANKS");
    fork_on_exec           = 0;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. \\(worker:", "returned result string");

    /* non existing command 2 */
    fork_on_exec = 1;
    exec_job->command_line = strdup("./THANKS 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 126 is out of bounds. Make sure the plugin you're trying to run is executable. \\(worker:", "returned result string");



    /*****************************************
     * unknown exit code 1
     */
    exec_job->command_line = strdup("./t/rc 5");
    fork_on_exec           = 1;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 5 is out of bounds. \\(worker:", "returned result string");

    /* unknown exit code 2 */
    fork_on_exec = 0;
    exec_job->command_line = strdup("./t/rc 5 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 5 is out of bounds. \\(worker:", "returned result string");

    /* unknown exit code 3 */
    exec_job->command_line = strdup("./t/rc 128 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 128 is out of bounds. Plugin exited by signal signal 0. \\(worker:", "returned result string");

    /* unknown exit code 4 */
    exec_job->command_line = strdup("./t/rc 137 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 137 is out of bounds. Plugin exited by signal SIGKILL. \\(worker:", "returned result string");

    /* unknown exit code 5 */
    exec_job->command_line = strdup("./t/rc 255 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 255 is out of bounds. \\(worker:", "returned result string");


    /*****************************************
     * signaled exit code SIGINT
     */
    exec_job->command_line = strdup("./t/killer INT");
    fork_on_exec           = 1;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 130 is out of bounds. Plugin exited by signal SIGINT. \\(worker:", "returned result string");

    /* signaled exit code SIGINT 2 */
    fork_on_exec = 0;
    exec_job->command_line = strdup("./t/killer INT 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "CRITICAL: Return code of 130 is out of bounds. Plugin exited by signal SIGINT. \\(worker:", "returned result string");



    /*****************************************
     * timed out check
     */
    exec_job->command_line = strdup("./t/sleep 30");
    exec_job->timeout      = 2;
    fork_on_exec           = 1;

    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "\\(Service Check Timed Out On Worker: ", "returned result string");

    /* timed out check 2 */
    fork_on_exec = 0;
    exec_job->command_line = strdup("./t/sleep 30 2>&1");
    execute_safe_command(exec_job, fork_on_exec, hostname);
    cmp_ok(exec_job->return_code, "==", 2, "cmd '%s' returns rc 2", exec_job->command_line);
    like(exec_job->output, "\\(Service Check Timed Out On Worker: ", "returned result string");

    /* reset timeout */
    exec_job->timeout      = 30;


    /* clean up */
    free_job(exec_job);

    return exit_status();
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
