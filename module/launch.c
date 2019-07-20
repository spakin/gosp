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
  char *idx;            /* Pointer into a string */
  apr_status_t status;  /* Status of an APR call */

  /* Construct a socket name from the Gosp filename. */
  status = apr_filepath_merge(&sock_name, run_dir, r->canonical_filename,
                              APR_FILEPATH_SECUREROOT|APR_FILEPATH_NOTRELATIVE,
                              r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(NULL, APLOG_ALERT, status,
                 "Failed to securely merge %s and %s", run_dir, r->canonical_filename);
  idx = rindex(sock_name, '.');
  if (idx != NULL)
    *idx = '\0';
  sock_name = apr_pstrcat(r->pool, sock_name, ".sock", NULL);
  return sock_name;
}


/* Create a socket to associate with a given Go Server Page.  Create all
 * directories needed along the way.  Return NULL on failure. */
apr_socket_t *create_socket(request_rec *r, const char *run_dir)
{
  apr_socket_t *sock;   /* The socket proper */
  char *sock_name;      /* Socket filename */
  char *sock_dir;       /* Directory component of the socket filename */
  char *idx;            /* Pointer into a string */
  apr_status_t status;  /* Status of an APR call */

  /* Construct a socket name from the Gosp filename. */
  status = apr_filepath_merge(&sock_name, run_dir, r->canonical_filename,
                              APR_FILEPATH_SECUREROOT|APR_FILEPATH_NOTRELATIVE,
                              r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(NULL, APLOG_ALERT, status,
                 "Failed to securely merge %s and %s", run_dir, r->canonical_filename);
  idx = rindex(sock_name, '.');
  if (idx != NULL)
    *idx = '\0';
  sock_name = apr_pstrcat(r->pool, sock_name, ".sock", NULL);

  /* Create all directories leading up to the socket base name. */
  sock_dir = dirname(apr_pstrdup(r->pool, sock_name));
  status = apr_dir_make_recursive(sock_dir,
                                  APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                                  r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(NULL, APLOG_ALERT, status,
                 "Failed to create directory %s", sock_dir);

  /* Remove any existing socket (or other file) with the same name. */
  (void) apr_file_remove(sock_name, r->pool);

  /* Create a Unix-domain stream socket. */
  status = apr_socket_create(&sock, APR_UNIX, SOCK_STREAM, APR_PROTO_TCP, r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(NULL, APLOG_ALERT, status,
                 "Failed to create socket %s", sock_name);
  return sock;
}
