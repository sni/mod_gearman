/******************************************************************************
 *
 * mod_gearman.h - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <common.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define GM_PERFDATA_QUEUE    "perfdata"

#define NSCORE

/* include some Nagios stuff as well */
#include "nagios/nagios.h"
#include "nagios/neberrors.h"
#include "nagios/nebstructs.h"
#include "nagios/nebcallbacks.h"
#include "nagios/broker.h"
#include "nagios/macros.h"


/* include the gearman libs */
#include <libgearman/gearman.h>

/* functions */
int nebmodule_init( int, char *, nebmodule * );
int nebmodule_deinit( int, int );

/* global variables */
char * mod_gm_opt_server[GM_LISTSIZE];
int    mod_gm_opt_debug_level;
char * mod_gm_opt_result_queue;
int    mod_gm_opt_timeout;
int    mod_gm_opt_result_workers;
char * mod_gm_opt_crypt_key;