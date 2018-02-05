/************************************************************************
 *
 * Nagios Locations Header File
 * Written By: Ethan Galstad (egalstad@nagios.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************/

#define DEFAULT_TEMP_FILE			"/var/spool/nagios/tempfile"
#define DEFAULT_TEMP_PATH                       "/tmp"
#define DEFAULT_CHECK_RESULT_PATH		"/var/spool/nagios/checkresults"
#define DEFAULT_STATUS_FILE			"/var/log/nagios/status.dat"
#define DEFAULT_LOG_FILE			"/var/log/nagios/nagios.log"
#define DEFAULT_LOG_ARCHIVE_PATH		"/var/log/nagios/archives/"
#define DEFAULT_DEBUG_FILE                      "/var/log/nagios/nagios.debug"
#define DEFAULT_COMMENT_FILE			"/var/log/nagios/comments.dat"
#define DEFAULT_DOWNTIME_FILE			"/var/log/nagios/downtime.dat"
#define DEFAULT_RETENTION_FILE			"/var/log/nagios/retention.dat"
#define DEFAULT_COMMAND_FILE			"/var/spool/nagios/cmd/nagios.cmd"
#define DEFAULT_QUERY_SOCKET                    "/var/spool/nagios/cmd/nagios.qh"
#define DEFAULT_CONFIG_FILE			"/etc/nagios/nagios.cfg"
#define DEFAULT_PHYSICAL_HTML_PATH		"/usr/share/nagios/html"
#define DEFAULT_URL_HTML_PATH			"/nagios"
#define DEFAULT_PHYSICAL_CGIBIN_PATH		"/usr/lib64/nagios/cgi"
#define DEFAULT_URL_CGIBIN_PATH			"/nagios/cgi-bin"
#define DEFAULT_CGI_CONFIG_FILE			"/etc/nagios/cgi.cfg"
#define DEFAULT_LOCK_FILE			"/var/run/nagios/nagios.pid"
#define DEFAULT_OBJECT_CACHE_FILE		"/var/spool/nagios/objects.cache"
#define DEFAULT_PRECACHED_OBJECT_FILE		"/var/spool/nagios/objects.precache"
#define DEFAULT_EVENT_BROKER_FILE		"/var/spool/nagios/broker.socket"
