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

#define MOD_GM_NEB  /**< set mod_gearman neb features */
#define NSCORE      /**< enable core features         */

#include "utils.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define GM_PERFDATA_QUEUE    "perfdata"  /**< default performance data queue */

/** @file
 *  @brief Mod-Gearman NEB Module
 *  @addtogroup mod_gearman_neb_module NEB Module
 *
 * The Mod-Gearman NEB module loads into the core and intercepts scheduled host
 * and service checks as well as eventhander jobs.
 * The module start a single new thread which acts as gearman client and worker.
 * The client is used to send new jobs into the gearman queues (functions). The
 * worker listens on the result queue and puts back the finished results.
 * Before the core reaps the result they will be merged together with the ones
 * from gearman.
 *
 * @{
 */

/* include some libnaemon */
#include "naemon/lib/libnaemon.h"
#include "naemon/nebmods.h"
#include "naemon/objects.h"
#include "naemon/utils.h"
#include "naemon/checks.h"
#include "naemon/neberrors.h"
#include "naemon/nebstructs.h"
#include "naemon/logging.h"
#include "naemon/broker.h"
#include "naemon/events.h"

/* include the gearman libs */
#include <libgearman/gearman.h>

/** main NEB module init function
 *
 * this function gets initally called when loading the module
 *
 * @param[in] flags  - module flags
 * @param[in] args   - module arguments from the core config
 * @param[in] handle - our module handle
 *
 * @return Zero on success, or a non-zero error value.
 */
int nebmodule_init( int flags, char * args, nebmodule * handle);

/** NEB module deinit function
 *
 * this function gets called before unloading the module from the core
 *
 * @param[in] flags  - module flags
 * @param[in] reason - reason for unloading the module
 *
 * @return nothing
 */
int nebmodule_deinit( int flags, int reason );

/** adds check result to result list
 *
 * @param[in] newcheckresult - new checkresult structure to add to list
 *
 * @return nothing
 */
void mod_gm_add_result_to_list(check_result * newcheckresult);

/**
 * @}
 */
