/*
    mini_epn.c
*/

#include <EXTERN.h>
#include <perl.h>
#include  "../include/nagios/epn_nagios.h"

#define MAX_INPUT_CHARS 1024

#define P1FILE DATADIR"/mod_gearman/mod_gearman_p1.pl"

static PerlInterpreter *my_perl = NULL;

int run_epn(char *command_line);

int main(int argc, char **argv) {
    struct stat stat_buf;
    char *p1 = P1FILE;
    // try fallback p1 file
    if(stat(P1FILE, &stat_buf) != 0 && stat("worker/mod_gearman_p1.pl", &stat_buf) == 0 ) {
        p1 = "worker/mod_gearman_p1.pl";
    }
    char *embedding[] = { "", p1 };
    char command_line[MAX_INPUT_CHARS];
    int exitstatus;

    /* usage? */
    if(argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf("Mod-Gearman Mini-ePN:\n");
        printf("\n");
        printf("Usage: %s [perl_plugin [arguments]]\n", argv[0]);
        printf("\n");
        printf("test perl plugins as if they were run by ePN.\n");
        exit(3);
    }

    if((my_perl = perl_alloc()) == NULL) {
        printf("%s\n", "Error: Could not allocate memory for embedded Perl interpreter!");
        exit(1);
    }
    perl_construct(my_perl);
    exitstatus = perl_parse(my_perl, xs_init, 2, embedding, NULL);
    if(!exitstatus) {
        exitstatus = perl_run(my_perl);
        if(argc > 1) {
            int x;
            command_line[0] = '\0';
            for(x=1; x<argc; x++) {
                strncat(command_line, argv[x], MAX_INPUT_CHARS - 1);
                if(argc != x) {
                    strncat(command_line, " ", MAX_INPUT_CHARS - 1);
                }
            }
            exitstatus = run_epn(command_line);
        } else {
            while(printf("Enter file name: ") && fgets(command_line, MAX_INPUT_CHARS - 1, stdin)) {
                exitstatus = run_epn(command_line);
            }
        }

        PL_perl_destruct_level = 0;
        perl_destruct(my_perl);
        perl_free(my_perl);
        exit(exitstatus);
    }
    return 0;
}

int run_epn(char *command_line) {
    SV *plugin_hndlr_cr;
    STRLEN n_a;
    int count = 0 ;
    char fname[MAX_INPUT_CHARS];
    char *args[] = {"", "0", "", "", NULL };
    int pclose_result;
    char *plugin_output ;


    dSP;

    command_line[strlen(command_line) - 1] = '\0';

    strncpy(fname, command_line, strcspn(command_line, " "));
    fname[strcspn(command_line, " ")] = '\x0';
    args[0] = fname ;
    args[3] = command_line + strlen(fname) + 1 ;

    args[2] = "";

    /* call our perl interpreter to compile and optionally cache the command */

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

    XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
    XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
    XPUSHs(sv_2mortal(newSVpv(args[2], 0)));
    XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

    PUTBACK;

    count = call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);

    SPAGAIN;

    /* check return status  */
    if(SvTRUE(ERRSV)) {
        (void) POPs;

        pclose_result = -2;
        printf("embedded perl ran %s with error %s\n", fname, SvPVX(ERRSV));
        return 1;
        }
    else {
        plugin_hndlr_cr = newSVsv(POPs);

        PUTBACK;
        FREETMPS;
        LEAVE;
        }

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);

    XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
    XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
    XPUSHs(plugin_hndlr_cr);
    XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

    PUTBACK;

    count = perl_call_pv("Embed::Persistent::run_package", G_EVAL | G_ARRAY);

    SPAGAIN;

    plugin_output = POPpx ;
    pclose_result = POPi ;

    printf("plugin return code: %d\n", pclose_result);
    printf("perl plugin output: '%s'\n", plugin_output);

    PUTBACK;
    FREETMPS;
    LEAVE;

    alarm(0);
    return 0;
}
