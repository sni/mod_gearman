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

typedef enum {
  GM_WORKER_OPTIONS_NONE=   0,
  GM_WORKER_OPTIONS_DATA=   (1 << 0),
  GM_WORKER_OPTIONS_STATUS= (1 << 1),
  GM_WORKER_OPTIONS_UNIQUE= (1 << 2)
} gm_worker_options_t;

void *result_worker();
void *get_results(gearman_job_st *, void *, size_t *, gearman_return_t *);
