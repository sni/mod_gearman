/*****************************************************************************
 *
 * mod_gearman.h - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

/* constants */
#define MOD_GEARMAN_VERSION     "0.1"
#define ENABLED                     1
#define BUFFERSIZE               4096
#define LISTSIZE                  256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* include some Nagios stuff as well */
#include "nagios/nagios.h"
#include "nagios/neberrors.h"
#include "nagios/nebstructs.h"
#include "nagios/nebcallbacks.h"
#include "nagios/broker.h"

/* include the gearman libs */
#include <libgearman/gearman.h>

/* functions */
int nebmodule_init( int, char *, nebmodule * );
int nebmodule_deinit( int, int );

/* global variables */
char * gearman_opt_server[LISTSIZE];
int    gearman_opt_debug_level;
char * gearman_opt_result_queue;
int    gearman_opt_timeout;
