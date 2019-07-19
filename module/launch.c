/**************************************************
 * Launch a gosp2go server from the Apache module *
 *                                                *
 * By Scott Pakin <scott+gosp@pakin.org>          *
 *************************************************/

#include "gosp.h"

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
  sock_name = apr_pstrcat(r->pool, run_dir, r->canonical_filename, NULL);
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
