/*****************************************
 * Various utility functions for use by  *
 * the Go Server Pages Apache module     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Return 1 if a given directory, named in the Apache configuration file, is
 * valid, creating it if necessary.  Otherwise, log an error message and return
 * 0.  If the directory name is NULL, assign it a default value. */
int prepare_config_directory(request_rec *r, const char *dir_type,
                             const char **dir_name, const char *default_name,
                             const char *config_name)
{
  apr_finfo_t finfo;    /* File information for the directory */
  apr_status_t status;  /* Return value from a file operation */

  /* If the directory wasn't specified in the configuration file, use a default
   * directory. */
  if (*dir_name == NULL) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "The Apache configuration file does not specify %s", config_name);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "Using %s as Gosp's %s directory", default_name, dir_type);
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
      REPORT_ERROR(0, APLOG_ALERT, status,
                   "Failed to create %s directory %s", dir_type, *dir_name);
    status = apr_stat(&finfo, *dir_name, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(0, APLOG_ALERT, status,
                   "Failed to query %s directory %s", dir_type, *dir_name);
  }
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(0, APLOG_ALERT, status,
                 "Gosp %s directory %s is not a directory", dir_type, *dir_name);

  /* Everything is okay. */
  return 1;
}

/* Create a directory hierarchy in which to store the given file.
 * Return an APR status code. */
apr_status_t create_directories_for(request_rec *r, const char *fname)
{
  char *dir_name;       /* Directory containing fname */
  apr_finfo_t finfo;    /* File information for the directory */
  apr_status_t status;  /* Return value from an APR operation */

  /* Check if the directory exists and is a directory. */
  dir_name = dirname(apr_pstrdup(r->pool, fname));
  status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, status, r->server,
                 "Failed to query directory %s; creating it", dir_name);
    status =
      apr_dir_make_recursive(dir_name,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      return status;
    status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      return status;
  }
  if (finfo.filetype != APR_DIR)
    return APR_ENOTDIR;

  /* Everything is okay. */
  return APR_SUCCESS;
}
