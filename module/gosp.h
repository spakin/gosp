/******************************************
 * Header file for the Gosp Apache module *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#ifndef _GOSP_H
#define _GOSP_H

/* Include all required header files here. */
#include <libgen.h>
#include <strings.h>
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_file_info.h"
#include "apr_network_io.h"
#include "apr_strings.h"

/* Define some default directories. */
#define DEFAULT_CACHE_DIR "/var/cache/apache2/mod_gosp"
#define DEFAULT_RUN_DIR "/tmp/mod_gosp"

/* Declare a type for our configuration options. */
typedef struct {
  const char *cache_dir;   /* Cache directory, for storing generated executables */
  const char *run_dir;     /* Run directory, for storing Unix-domain sockets */
} config_t;

/* Define a macro that checks an error code and, on error, logs a
 * message and returns 0. */
#define REPORT_ERROR(RETVAL, LEVEL, STATUS, ...)                        \
do {                                                                    \
  if (STATUS == APR_SUCCESS)                                            \
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|LEVEL, STATUS, r->server, __VA_ARGS__); \
  else                                                                  \
    ap_log_error(APLOG_MARK, LEVEL, STATUS, r->server, __VA_ARGS__);    \
  return RETVAL;                                                        \
} while (0)

/* Declare functions that will be called cross-file. */
extern int prepare_directory(request_rec *r, const char *dir_type, const char **dir_name, const char *default_name);
extern apr_socket_t *create_socket(request_rec *r, const char *run_dir);

#endif
