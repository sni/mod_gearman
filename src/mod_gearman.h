/*****************************************************************************
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
char * gearman_opt_server[GM_LISTSIZE];
int    gearman_opt_debug_level;
char * gearman_opt_result_queue;
int    gearman_opt_timeout;
int    gearman_opt_result_workers;
