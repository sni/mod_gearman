/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "mod_gearman.h"

#include <libgearman/gearman.h>

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

void *result_worker(void *);
void *get_results(gearman_job_st *, void *, size_t *, gearman_return_t *);

typedef struct result_worker_arg_struct {
    int      timeout;
    char *   result_queue_name;
    char *   server[LISTSIZE];
} result_worker_arg;