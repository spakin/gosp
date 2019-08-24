/******************************************
 * Header file for the Gosp Apache module *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#ifndef _GOSP_H
#define _GOSP_H

/* Include all required header files here. */
#include <libgen.h>
#include <stdarg.h>
#include <sys/uio.h>
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "ap_config.h"
#include "apr_env.h"
#include "apr_file_info.h"
#include "apr_network_io.h"
#include "apr_strings.h"

/* Define our default work directory. */
#define DEFAULT_WORK_DIR "/var/cache/apache2/mod_gosp"

/* Ensure GOSP2GO is defined, typically on the command line. */
#ifndef GOSP2GO
# error GOSP2GO needs to be defined.
#endif

/* Define some status codes for the functions we define. */
#define GOSP_STATUS_OK          0    /* Function succeeded */
#define GOSP_STATUS_NEED_ACTION 1    /* Function failed but may succeed if the caller takes some action and retries */
#define GOSP_STATUS_FAIL        2    /* Function experienced a presumably permanent failure */

/* Define a time in microseconds we're willing to wait to receive a
 * chunk of data from a Gosp server. */
#define GOSP_RESPONSE_TIMEOUT 5000000

/* Define a type corresponding the above. */
typedef int gosp_status_t;

/* Declare a type for our configuration options. */
typedef struct {
  const char *work_dir;    /* Work directory, for storing Gosp-generated files */
  apr_uid_t user_id;       /* User ID when server answers requests */
  apr_gid_t group_id;      /* Group ID when server answers requests */
} gosp_config_t;

/* Define access permissions for any directories we create. */
#define GOSP_DIR_PERMS                                  \
  APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE | \
  APR_FPROT_GREAD|APR_FPROT_GEXECUTE |                  \
  APR_FPROT_WREAD|APR_FPROT_WEXECUTE

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
extern gosp_status_t connect_socket(apr_socket_t **sock, request_rec *r, const char *sock_name);
extern gosp_status_t launch_gosp_process(request_rec *r, const char *run_dir, const char *sock_name);
extern gosp_status_t create_directories_for(request_rec *r, const char *fname, int is_dir);
extern char *concatenate_filepaths(request_rec *r, ...);
extern int is_newer_than(request_rec *r, const char *first, const char *second);
extern gosp_status_t compile_gosp_server(request_rec *r, const char *work_dir);
extern gosp_status_t send_request(apr_socket_t *sock, request_rec *r);
extern gosp_status_t send_termination_request(apr_socket_t *sock, request_rec *r);
extern gosp_status_t process_response(apr_socket_t *sock, request_rec *r);

#endif
