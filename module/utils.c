/*****************************************
 * Various utility functions for use by  *
 * the Go Server Pages Apache module     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Return 1 if a given directory is valid, creating it if necessary.
 * Otherwise, log an error message and return 0. */
int prepare_directory(request_rec *r, const char *dir_type, const char **dir_name, const char *default_name)
{
  apr_finfo_t finfo;    /* File information for the directory */
  apr_status_t status;  /* Return value from a file operation */

  /* If the directory wasn't specified in the configuration file, use a default
   * directory. */
  if (*dir_name == NULL) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "A Gosp %s directory was not specified in the Apache configuration file", dir_type);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "Using %s as the %s directory", default_name, dir_type);
    *dir_name = default_name;
  }

  /* Ensure that the directory exists and is a directory. */
  status = apr_stat(&finfo, *dir_name, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_NOTICE, status, r->server,
                 "Failed to query %s directory %s; creating it", dir_type, *dir_name);
    status =
      apr_dir_make_recursive(*dir_name,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to create %s directory %s", dir_type, *dir_name);
    status = apr_stat(&finfo, *dir_name, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to query %s directory %s", dir_type, *dir_name);
  }
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(APLOG_ALERT, APR_SUCCESS,
                 "Gosp %s directory %s is not a directory", dir_type, *dir_name);

  /* Everything is okay. */
  return 1;
}
