/*****************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "mod_gearman.h"

#include <libgearman/gearman.h>
#include "nagios/nagios.h"

void *result_worker(void *);
void *get_results(gearman_job_st *, void *, size_t *, gearman_return_t *);

typedef struct result_worker_arg_struct {
    int      timeout;
    char *   server[LISTSIZE];
} result_worker_arg;
