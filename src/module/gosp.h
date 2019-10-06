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
#include <unistd.h>
#include <inttypes.h>
#include <stdarg.h>
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "ap_config.h"
#include "apr_env.h"
#include "apr_file_info.h"
#include "apr_global_mutex.h"
#include "apr_network_io.h"
#include "apr_strings.h"
#if AP_NEED_SET_MUTEX_PERMS
# include "unixd.h"
#endif

/* Define our default work directory. */
#define DEFAULT_WORK_DIR "/var/cache/apache2/mod_gosp"

/* Ensure GOSP2GO is defined, typically on the command line. */
#ifndef GOSP2GO
# error GOSP2GO needs to be defined as the full filespec for gosp2go.
#endif

/* Ensure GOSP_SERVER is defined, typically on the command line. */
#ifndef GOSP_SERVER
# error GOSP_SERVER needs to be defined as the full filespec for gosp-server.
#endif

/* Ensure DEFAULT_GO_COMMAND is defined, typically on the command line. */
#ifndef DEFAULT_GO_COMMAND
# error DEFAULT_GO_COMMAND needs to be defined as the Go compiler executable.
#endif

/* Ensure DEFAULT_GO_PATH is defined, typically on the command line. */
#ifndef DEFAULT_GO_PATH
# error DEFAULT_GO_PATH needs to be defined as the default GOPATH.
#endif

/* Define some status codes for the functions we define. */
#define GOSP_STATUS_OK          0    /* Function succeeded */
#define GOSP_STATUS_NEED_ACTION 1    /* Function failed but may succeed if the caller takes some action and retries */
#define GOSP_STATUS_FAIL        2    /* Function experienced a presumably permanent failure */

/* Define a number of wait-time values (all in microseconds). */
#define GOSP_SECONDS 1000000
#define GOSP_RESPONSE_TIMEOUT (600*GOSP_SECONDS)  /* Time to wait to receive a chunk of data from a Gosp server */
#define GOSP_LOCK_WAIT_TIME    (10*GOSP_SECONDS)  /* Time to wait to acquire a lock */
#define GOSP_LAUNCH_WAIT_TIME   (3*GOSP_SECONDS)  /* Time to wait for a Gosp server to launch */
#define GOSP_EXIT_WAIT_TIME     (1*GOSP_SECONDS)  /* Time to wait for a Gosp server to exit */

/* Define the maximum size of a POST request that we'll allow. */
#ifndef GOSP_MAX_POST_SIZE
# define GOSP_MAX_POST_SIZE 1048576
#endif

/* Define a type corresponding the above. */
typedef int gosp_status_t;

/* Declare a type for our per-server configuration options. */
typedef struct {
  const char *work_dir;        /* Work directory, for storing Gosp-generated files */
  apr_uid_t user_id;           /* User ID when server answers requests */
  apr_gid_t group_id;          /* Group ID when server answers requests */
  apr_global_mutex_t *mutex;   /* Global lock to serialize operations*/
  const char *lock_name;       /* Name of a file to back the mutex, if needed */
} gosp_server_config_t;

/* Declare a type for our per-context configuration options. */
typedef struct {
  const char *context;         /* String uniquely naming the context */
  const char *go_cmd;          /* Go compiler executable filespec */
  const char *gosp_server;     /* gosp-server executable filespec */
  const char *go_path;         /* Value to assign to GOPATH when compiling Gosp pages */
  const char *max_idle;        /* Maximum idle time before a Gosp server automatically exits */
  const char *max_top;         /* Maximum number of top-level blocks allowed per Gosp page */
  const char *allowed_imports; /* Comma-separated list of packages that can be imported */
} gosp_context_config_t;

/* Define access permissions for any files and directories we create. */
#define GOSP_FILE_PERMS                                 \
  APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_GREAD|APR_FPROT_WREAD
#define GOSP_DIR_PERMS                                  \
  APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE | \
  APR_FPROT_GREAD|APR_FPROT_GEXECUTE |                  \
  APR_FPROT_WREAD|APR_FPROT_WEXECUTE

/* Define a macro that logs a server-level error message and returns a given
 * return value. */
#define REPORT_SERVER_ERROR(RETVAL, LEVEL, STATUS, ...)                    \
do {                                                                       \
  if (STATUS == APR_SUCCESS)                                               \
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|LEVEL, STATUS, s, __VA_ARGS__); \
  else                                                                     \
    ap_log_error(APLOG_MARK, LEVEL, STATUS, s, __VA_ARGS__);               \
  return RETVAL;                                                           \
} while (0)

/* Define a macro that logs a request-level error message and returns a given
 * return value. */
#define REPORT_REQUEST_ERROR(RETVAL, LEVEL, STATUS, ...)                    \
do {                                                                        \
  if (STATUS == APR_SUCCESS)                                                \
    ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|LEVEL, STATUS, r, __VA_ARGS__); \
  else                                                                      \
    ap_log_rerror(APLOG_MARK, LEVEL, STATUS, r, __VA_ARGS__);               \
  return RETVAL;                                                            \
} while (0)

/* Declare variables and functions that will be accessed cross-file. */
extern module AP_MODULE_DECLARE_DATA gosp_module;
extern char *concatenate_filepaths(server_rec *s, apr_pool_t *pool, ...);
extern const char **append_string(apr_pool_t *p, const char *const *list, const char *str);
extern gosp_status_t acquire_global_lock(server_rec *s);
extern gosp_status_t compile_gosp_server(request_rec *r, const char *plugin_name);
extern gosp_status_t connect_socket(request_rec *r, const char *sock_name, apr_socket_t **sock);
extern gosp_status_t create_directories_for(server_rec *s, apr_pool_t *pool, const char *fname, int is_dir);
extern gosp_status_t kill_gosp_server(request_rec *r, const char *sock_name, const char *plugin_name);
extern gosp_status_t launch_gosp_server(request_rec *r, const char *plugin_name, const char *sock_name);
extern gosp_status_t receive_response(request_rec *r, apr_socket_t *sock, char **response, size_t *resp_len);
extern gosp_status_t release_global_lock(server_rec *s);
extern gosp_status_t send_request(request_rec *r, apr_socket_t *sock);
extern gosp_status_t send_termination_request(request_rec *r, const char *sock_name);
extern gosp_status_t simple_request_response(request_rec *r, const char *sock_name);
extern int is_newer_than(request_rec *r, const char *first, const char *second);

#endif
