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

/* Return the name of a socket to use to communicate with the Gosp server
 * associated with the current URL.  Return NULL on failure, */
char *get_socket_name(request_rec *r, const char *run_dir)
{
  char *sock_name;      /* Socket filename */
  char *req_name;       /* Requested filename */
  char *idx;            /* Pointer into a string */
  apr_status_t status;  /* Status of an APR call */

  /* Construct a socket name from the Gosp filename. */
  req_name = r->canonical_filename;
  if (req_name[0] == '/')
    req_name++;   /* APR_FILEPATH_SECUREROOT won't allow merging to an absolute pathname. */
  status = apr_filepath_merge(&sock_name, run_dir, req_name,
                              APR_FILEPATH_SECUREROOT|APR_FILEPATH_NOTRELATIVE,
                              r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(NULL, APLOG_ALERT, status,
                 "Failed to securely merge %s and %s", run_dir, r->canonical_filename);
  sock_name = apr_pstrcat(r->pool, sock_name, ".sock", NULL);
  return sock_name;
}

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
