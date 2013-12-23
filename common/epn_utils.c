/******************************************************************************
 *
 * mod_gearman - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein - sven.nierlein@consol.de
 *
 * This file is part of mod_gearman.
 *
 *  mod_gearman is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mod_gearman is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mod_gearman.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include "config.h"
#include "utils.h"
#include "check_utils.h"
#include "epn_utils.h"
#include "worker_client.h"
#include "gearman_utils.h"

#ifdef EMBEDDEDPERL
#include <EXTERN.h>
#include <perl.h>
#include  "epn_mod_gearman.h"
int use_embedded_perl            = TRUE;
int deinit_rc                    = 0;
static PerlInterpreter *my_perl  = NULL;
extern int current_child_pid;
extern int enable_embedded_perl;
extern int use_embedded_perl_implicitly;
extern int use_perl_cache;
extern char *p1_file;
#endif

int run_epn_check(char *processed_command, char **ret, char **err) {
#ifdef EMBEDDEDPERL
    int retval;
    int pipe_stdout[2], pipe_stderr[2];
    char fname[512]="";
    char *args[5]={"",NULL, "", "", NULL };
    char *perl_plugin_output=NULL;
    SV *plugin_hndlr_cr;
    int count;
    FILE *fp;
    pid_t pid;
    sigset_t mask;

    int use_epn=FALSE;
    if(my_perl == NULL) {
        gm_log(GM_LOG_ERROR, "Embedded Perl has to be initialized before running the first check\n");
        _exit(STATE_UNKNOWN);
    }
#ifdef aTHX
    dTHX;
#endif
    dSP;

    /* get filename  component of command */
    strncpy(fname,processed_command,strcspn(processed_command," "));
    fname[strcspn(processed_command," ")]='\x0';

    /* should we use the embedded Perl interpreter to run this script? */
    use_epn=file_uses_embedded_perl(fname);
    if(use_epn == FALSE)
        return GM_NO_EPN;

    gm_log(GM_LOG_DEBUG, "Using Embedded Perl interpreter for: %s\n", fname);

    args[0]=fname;
    args[1]=use_perl_cache==GM_ENABLED ? "0" : "1";
    args[2]="";

    if(strchr(processed_command,' ')==NULL)
        args[3]="";
    else
        args[3]=processed_command+strlen(fname)+1;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVpv(args[0],0)));
    XPUSHs(sv_2mortal(newSVpv(args[1],0)));
    XPUSHs(sv_2mortal(newSVpv(args[2],0)));
    XPUSHs(sv_2mortal(newSVpv(args[3],0)));
    PUTBACK;

    /* call our perl interpreter to compile and optionally cache the command */
    call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);
    SPAGAIN ;

    /* compile failed */
    if( SvTRUE(ERRSV) ){
        /* remove the top element of the Perl stack (undef) */
        (void) POPs ;
        perl_plugin_output=SvPVX(ERRSV);
        if(perl_plugin_output == NULL)
            *ret = strdup("(Embedded Perl failed to compile)");
        else
            *ret = gm_escape_newlines(perl_plugin_output, GM_ENABLED);
        *err = strdup("");
        gm_log( GM_LOG_TRACE, "Embedded Perl failed to compile %s, compile error %s - skipping plugin\n", fname, perl_plugin_output);
        return 768;
    }
    else {
        plugin_hndlr_cr=newSVsv(POPs);
        PUTBACK;
        FREETMPS;
        LEAVE;
        gm_log( GM_LOG_TRACE, "Embedded Perl successfully compiled %s and returned code ref to plugin handler\n", fname );
    }
    /* now run run the check */
    if(pipe(pipe_stdout)) {
        gm_log( GM_LOG_ERROR, "error creating pipe: %s\n", strerror(errno));
        _exit(STATE_UNKNOWN);
    }
    if(pipe(pipe_stderr)) {
        gm_log( GM_LOG_ERROR, "error creating pipe: %s\n", strerror(errno));
        _exit(STATE_UNKNOWN);
    }
    if((pid=fork())<0){
        gm_log( GM_LOG_ERROR, "fork error\n");
        _exit(STATE_UNKNOWN);
    }
    else if(!pid) {
        /* child process */

        /* remove all customn signal handler */
        sigfillset(&mask);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        gm_log( GM_LOG_TRACE, "Embedded Perl Child\n" );
        if((dup2(pipe_stderr[1],STDERR_FILENO)<0)){
            gm_log( GM_LOG_ERROR, "dup2 error\n");
            _exit(STATE_UNKNOWN);
        }

        close(pipe_stdout[0]);
        close(pipe_stderr[1]);
        current_child_pid = getpid();

        /* remove parents signal handler */
        signal(SIGINT,  SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP,  SIG_DFL);
        signal(SIGPIPE, SIG_DFL);

        /* run the perl script */
        ENTER;
        SAVETMPS;
        PUSHMARK(SP);
        XPUSHs(sv_2mortal(newSVpv(args[0],0)));
        XPUSHs(sv_2mortal(newSVpv(args[1],0)));
        XPUSHs(plugin_hndlr_cr);
        XPUSHs(sv_2mortal(newSVpv(args[3],0)));
        PUTBACK;
        count=call_pv("Embed::Persistent::run_package", G_ARRAY);
        SPAGAIN;
        perl_plugin_output = POPpx;
        retval = POPi;

        if(perl_plugin_output!=NULL) {
            if(write(pipe_stdout[1], perl_plugin_output, strlen(perl_plugin_output)+1) <= 0)
                perror("write stdout");
        }

        PUTBACK;
        FREETMPS;
        LEAVE;

        /* free structures */
        mod_gm_free_opt(mod_gm_opt);
        deinit_embedded_perl(retval);

        _exit(retval);
    }

    /* parent */
    else {
        /* prepare stdout pipe reading */
        close(pipe_stdout[1]);
        fp=fdopen(pipe_stdout[0],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *ret = extract_check_result(fp, GM_DISABLED);
        fclose(fp);

        /* prepare stderr pipe reading */
        close(pipe_stderr[1]);
        fp=fdopen(pipe_stderr[0],"r");
        if(!fp){
            gm_log( GM_LOG_ERROR, "fdopen error\n");
            _exit(STATE_UNKNOWN);
        }
        *err = extract_check_result(fp, GM_ENABLED);
        fclose(fp);

        close(pipe_stdout[0]);
        close(pipe_stderr[0]);
        if(waitpid(pid,&retval,0)!=pid)
            retval=STATE_UNKNOWN;

        return retval;
    }
#endif
    return GM_NO_EPN;
}


/* checks to see if we should run a script using the embedded Perl interpreter */
int file_uses_embedded_perl(char *fname) {
#ifndef EMBEDDEDPERL
    return FALSE;
#else
    int line;
    FILE *fp = NULL;
    char buf[256] = "";

    if(enable_embedded_perl != TRUE)
        return FALSE;

    /* open the file, check if its a Perl script and see if we can use epn */
    fp = fopen(fname, "r");
    if(fp == NULL)
        return FALSE;

    /* grab the first line - we should see Perl. go home if not */
    if (fgets(buf, 80, fp) == NULL || strstr(buf, "/bin/perl") == NULL) {
        fclose(fp);
        return FALSE;
    }

    /* epn directives must be found in first ten lines of plugin */
    for(line = 1; line < 10; line++) {
        if(fgets(buf, sizeof(buf) - 1, fp) == NULL)
            break;

        buf[sizeof(buf) - 1] = '\0';

        /* skip lines not containing 'epn' directives */
        if(strstr(buf, "# nagios:") || strstr(buf, "# naemon:") || strstr(buf, "# icinga:")) {
            char *p;
            p = strstr(buf + 8, "epn");
            if (!p)
                continue;

            /*
            * we found it, so close the file and return
            * whatever it shows. '+epn' means yes. everything
            * else means no
            */
            fclose(fp);
            return *(p - 1) == '+' ? TRUE : FALSE;
        }
    }

    fclose(fp);

    return use_embedded_perl_implicitly;
#endif
}


/* initializes embedded perl interpreter */
int init_embedded_perl(char **env){
#ifdef EMBEDDEDPERL
    void **embedding;
    int exitstatus=0;
    int argc=2;
    argc=argc;
    struct stat stat_buf;

    /* make sure the P1 file exists... */
    if(p1_file==NULL || stat(p1_file,&stat_buf)!=0){
        use_embedded_perl=FALSE;
        gm_log(GM_LOG_ERROR,"Error: p1.pl file (%s) required for embedded Perl interpreter is missing!\n", p1_file);
    }

    else{
        embedding=(void **)malloc(2*sizeof(char *));
        if(embedding==NULL)
            return GM_ERROR;
        *embedding=strdup("");
        *(embedding+1)=strdup(p1_file);
        use_embedded_perl=TRUE;
        PERL_SYS_INIT3(&argc,&embedding,&env);
        if((my_perl=perl_alloc())==NULL){
            use_embedded_perl=FALSE;
            gm_log(GM_LOG_ERROR,"Error: Could not allocate memory for embedded Perl interpreter!\n");
        }
    }

    /* a fatal error occurred... */
    if(use_embedded_perl==FALSE){
        gm_log(GM_LOG_ERROR,"Bailing out due to errors encountered while initializing the embedded Perl interpreter.\n");
        return GM_ERROR;
    }

    perl_construct(my_perl);
    exitstatus=perl_parse(my_perl,xs_init,2,(char **)embedding,env);
    if(!exitstatus)
        exitstatus=perl_run(my_perl);
    free(*embedding);
    free(*(embedding+1));
#endif
    return GM_OK;
}

#ifdef EMBEDDEDPERL
/* catch sigsegv during deinitialzing and just exit */
void deinit_segv( int sig ) {
    _exit(deinit_rc);
}
#endif

/* closes embedded perl interpreter */
int deinit_embedded_perl(int rc) {
#ifdef EMBEDDEDPERL
    deinit_rc = rc;
    signal(SIGSEGV, deinit_segv);
    PL_perl_destruct_level=0;
    perl_destruct(my_perl);
    perl_free(my_perl);
    PERL_SYS_TERM();
    free(p1_file);
    signal(SIGSEGV, SIG_DFL);
#endif
    return GM_OK;
}
