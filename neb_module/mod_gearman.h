/*****************************************************************************
 *
 * mod_gearman.h - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#define  MOD_GEARMAN_VERSION     "0.1"

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

#define ENABLED        1
#define BUFFERSIZE  4096
#define LISTSIZE     256

/* functions */
int nebmodule_init(int, char *, nebmodule *);
static void register_neb_callbacks();
static void read_arguments(const char *);
static int handle_host_check(int,void *);
static int handle_svc_check(int,void *);
static int handle_eventhandler(int,void *);
static int create_gearman_client();
static char *get_target_worker(host *, service *);
static int handle_process_events(int, void *);
static void start_threads();

char * gearman_opt_server[LISTSIZE];
int gearman_opt_debug_level;
