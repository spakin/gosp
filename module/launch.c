/**************************************************
 * Launch a gosp2go server from the Apache module *
 *                                                *
 * By Scott Pakin <scott+gosp@pakin.org>          *
 *************************************************/

#include "gosp.h"

/* Return the filename of a Unix-domain stream socket to associate with a given
 * Go Server Page.  Create all directories needed along the way.  Return NULL
 * on failure. */
char *get_socket_name(request_rec *r, const char *run_dir)
{
  char *sock_name;      /* Socket filename */
  char *sock_dir;       /* Directory component of the socket filename */
  char *idx;            /* Pointer into a string */
  apr_finfo_t finfo;    /* File information */
  apr_status_t status;  /* Status of an APR call */
  
  /* Construct a socket name from the Gosp filename. */
  sock_name = apr_pstrcat(r->pool, run_dir, r->canonical_filename, NULL);
  idx = rindex(sock_name, '.');
  if (idx != NULL)
    *idx = '\0';
  sock_name = apr_pstrcat(r->pool, sock_name, ".sock", NULL);

  /* Return the socket name if the socket already exists. */
  status = apr_stat(&finfo, sock_name, APR_FINFO_TYPE, r->pool);
  if (status == APR_SUCCESS) {
    /* File exists and is a socket: we're done. */
    if (finfo.filetype == APR_SOCK)
      return sock_name;

    /* File exists but is not a socket: remove it. */
    status = apr_file_remove(sock_name, r->pool);
    if (status != APR_SUCCESS) {
      ap_log_error(APLOG_MARK, APLOG_ALERT, status, r->server,
		   "Failed to remove non-socket %s", sock_name);
      return NULL;
    }
  }

  /* The socket doesn't exist.  Create it. */
  sock_dir = dirname(apr_pstrdup(r->pool, sock_name));
  status = apr_dir_make_recursive(sock_dir,
				  APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
				  r->pool);
  if (status != APR_SUCCESS) {
    ap_log_error(APLOG_MARK, APLOG_ALERT, status, r->server,
		 "Failed to create directory %s", sock_dir);
    return NULL;
  }
  return sock_name;
}