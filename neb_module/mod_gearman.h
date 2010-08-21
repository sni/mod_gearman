/*****************************************************************************
 *
 * mod_gearman.h - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "nagios/include/nebmodules.h"
#include "nagios/include/nebcallbacks.h"

/* include some Nagios stuff as well */
#include "nagios/include/config.h"
#include "nagios/include/common.h"
#include "nagios/include/nagios.h"
#include "nagios/include/neberrors.h"
#include "nagios/include/objects.h"
#include "nagios/include/nebmodules.h"
#include "nagios/include/nebstructs.h"
#include "nagios/include/nebcallbacks.h"
#include "nagios/include/neberrors.h"
#include "nagios/include/broker.h"

/* include the gearman libs */
#include <libgearman/gearman.h>

#define ENABLED        1
#define BUFFERSIZE  4096
#define LISTSIZE     256

/* functions */
static void register_neb_callbacks();
int read_arguments(const char *);
int handle_host_check(int,void *);
int handle_svc_check(int,void *);
int handle_eventhandler(int,void *);
int create_gearman_client();
char *get_target_worker(host *, service *);
int handle_process_events(int, void *);
void start_threads();

static char * gearman_opt_server[LISTSIZE];
static int gearman_opt_debug_level;
