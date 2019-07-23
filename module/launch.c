/**************************************************
 * Launch a gosp2go server from the Apache module *
 *                                                *
 * By Scott Pakin <scott+gosp@pakin.org>          *
 *************************************************/

/*
 * TODO
 *
 * Flow should be as follows:
 *
 * 1) Get socket name.
 * 2) Try connecting to socket.
 *    If filename error,
 *  3) Create all subdirectories.
 *     Try connecting to socket.  Abort on failure.
 *    Else if failure/timeout,
 *  4) Relaunch server.
 *     If not found,
 *    6) Build server.  Abort on failure.
 *    7) Launch server.  Abort on failure.
 *  8) Try connecting to socket.  Abort on failure.
 */

#include "gosp.h"

/* Invoke an APR call as part of process launch.  On failure, log an error
 * message and return GOSP_LAUNCH_FAIL. */
#define LAUNCH_CALL(FCALL, ...)                                            \
do {                                                                       \
  status = FCALL;                                                          \
  if (status != APR_SUCCESS) {                                             \
    ap_log_error(APLOG_MARK, APLOG_ALERT, status, r->server, __VA_ARGS__); \
    return GOSP_LAUNCH_FAIL;                                               \
  }                                                                        \
} while (0)

/* Connect to a Unix-domain stream socket. */
apr_status_t connect_socket(apr_socket_t **sock, request_rec *r, const char *sock_name)
{
  apr_status_t status;    /* Status of an APR call */
  apr_sockaddr_t *sa;     /* Socket address corresponding to sock_name */

  /* Construct a socket address. */
  status = apr_sockaddr_info_get(&sa, sock_name, APR_UNIX, 0, 0, r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(status, APLOG_ALERT, status,
                 "Failed to construct a Unix-domain socket address from %s", sock_name);

  /* Create a Unix-domain stream socket. */
  status = apr_socket_create(sock, APR_UNIX, SOCK_STREAM, APR_PROTO_TCP, r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(status, APLOG_ALERT, status,
                 "Failed to create socket %s", sock_name);

  /* Connect to the socket we just created. */
  status = apr_socket_connect(*sock, sa);
  return status;
}

/* Launch a Go Server Page process to handle the current page.  Return
 * GOSP_LAUNCH_OK on success, GOSP_LAUNCH_NOTFOUND if the executable wasn't
 * found (and presumably need to be built), and GOSP_LAUNCH_FAIL if an
 * unexpected error occurred (and the request needs to be aborted). */
int launch_gosp_process(request_rec *r, const char *work_dir, const char *sock_name)
{
  char *server_name;      /* Name of the Gosp server executable */
  apr_proc_t proc;        /* Launched process */
  apr_procattr_t *attr;   /* Process attributes */
  const char **args;      /* Process command-line arguments */
  apr_status_t status;    /* Status of an APR call */

  /* Ensure we have a place to write the socket. */
  LAUNCH_CALL(create_directories_for(r, sock_name),
              "Creating a directory in which to write %s", sock_name);

  /* Prepare the process attributes. */
  LAUNCH_CALL(apr_procattr_create(&attr, r->pool),
              "Creating Gosp process attributes failed");
  LAUNCH_CALL(apr_procattr_detach_set(attr, 1),
              "Specifying that a Gosp process should detach from its parent");
  LAUNCH_CALL(apr_procattr_error_check_set(attr, 1),
              "Specifying that there should be extra error checking for a Gosp process");
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM_ENV),
              "Specifying that a Gosp process should inherit its parent's environment");

  /* Prefix the Gosp page name with the run directory to produce the name of
   * the Gosp server. */
  server_name = concatenate_filepaths(r, work_dir, "bin", r->canonical_filename, NULL);
  if (server_name == NULL)
    return GOSP_LAUNCH_FAIL;

  /* Spawn the Gosp process. */
  args = (const char **) apr_palloc(r->pool, 4*sizeof(char *));
  args[0] = server_name;
  args[1] = "-socket";
  args[2] = sock_name;
  args[3] = NULL;
  status = apr_proc_create(&proc, server_name, args, NULL, attr, r->pool);
  if (status == APR_SUCCESS)
    return GOSP_LAUNCH_OK;
  if (APR_STATUS_IS_ENOENT(status))
    return GOSP_LAUNCH_NOTFOUND;
  return GOSP_LAUNCH_FAIL;
}
