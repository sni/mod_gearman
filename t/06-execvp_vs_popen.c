#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <t/tap.h>
#include <common.h>
#include <utils.h>
#include <check_utils.h>
#ifdef EMBEDDEDPERL
#include <epn_utils.h>
#endif

#include <worker_dummy_functions.c>

mod_gm_opt_t *mod_gm_opt;
char hostname[GM_SMALLBUFSIZE];
volatile sig_atomic_t shmid;

char* my_tmpfile(void);
char* my_tmpfile() {
    char *sfn = strdup("/tmp/modgm.XXXXXX");
    int fd = -1;
    if ((fd = mkstemp(sfn)) == -1) {
        fprintf(stderr, "%s: %s\n", sfn, strerror(errno));
        free(sfn);
        return (NULL);
    }
    close(fd);
    return sfn;
}

/* check logfile for errors
 */
int check_logfile(char *logfile, char *match);
int check_logfile(char *logfile, char *match) {
    FILE * fp;
    char *line;
    int found = 0;

    fp = fopen(logfile, "r");
    if(fp == NULL) {
        perror(logfile);
        return(0);
    }
    line = malloc(GM_BUFFERSIZE);
    while(fgets(line, GM_BUFFERSIZE, fp) != NULL) {
        if(strstr(line, match) != NULL) {
            found++;
        }
    }
    fclose(fp);

    free(line);
    return(found);
}


int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv, __attribute__((unused)) char **env) {
    char *result, *error;
    char cmd[120];
    char logf[150];
    char * worker_logfile;
    int x, rc, matches;

    plan(3);

    /* set hostname */
    gethostname(hostname, GM_SMALLBUFSIZE-1);

    /* create options structure and set debug level */
    mod_gm_opt = malloc(sizeof(mod_gm_opt_t));
    set_default_options(mod_gm_opt);
    mod_gm_opt->debug_level = 4;
    mod_gm_opt->logmode     = GM_LOG_MODE_FILE;

    worker_logfile = my_tmpfile();
    snprintf(logf, 150, "logfile=%s", worker_logfile);
    rc = parse_args_line(mod_gm_opt, logf, 0);
    cmp_ok(rc, "==", GM_OK, "parsed %s option", logf);
    mod_gm_opt->logfile_fp = fopen(mod_gm_opt->logfile, "a+");
    if(mod_gm_opt->logfile_fp == NULL) {
        perror(mod_gm_opt->logfile);
        fail("failed to open logfile");
    }

#ifdef EMBEDDEDPERL
    char p1[150];
    snprintf(p1, 150, "--p1_file=worker/mod_gearman_p1.pl");
    parse_args_line(mod_gm_opt, p1, 0);
    init_embedded_perl(env);
#endif

    /* popen */
    gm_job_t * exec_job;
    exec_job = ( gm_job_t * )malloc( sizeof *exec_job );
    set_default_job(exec_job, mod_gm_opt);
    strcpy(cmd, "BLAH=BLUB /bin/hostname");
    run_check(cmd, &result, &error);
    free(result);
    free(error);
    matches = check_logfile(worker_logfile, "using popen");
    ok(matches == 1, "worker uses popen");

    /* execvp */
    strcpy(cmd, "/bin/hostname");
    run_check(cmd, &result, &error);
    free(result);
    free(error);
    mod_gm_opt->debug_level = 0;
    matches = check_logfile(worker_logfile, "using execvp");
    ok(matches == 1, "worker uses execvp");

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
    unlink(worker_logfile);
    free(worker_logfile);
    return(exit_status());
}

/* core log wrapper */
void write_core_log(char *data) {
    printf("core logger is not available for tests: %s", data);
    return;
}
