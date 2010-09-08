/******************************************************************************
 *
 * mod_gearman.c - distribute checks with gearman
 *
 * Copyright (c) 2010 Sven Nierlein
 *
 *****************************************************************************/

#include "mod_gearman.h"

#include <libgearman/gearman.h>
#include "nagios/nagios.h"

typedef struct {
    int id;
} worker_parm;

void *result_worker(void *);
int set_worker( gearman_worker_st *worker );
void *get_results( gearman_job_st *, void *, size_t *, gearman_return_t * );
