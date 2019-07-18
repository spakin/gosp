/*****************************************
 * Directory preparation for the Go      *
 * Server Pages Apache module            *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Return 1 if the cache directory is valid, creating it if necessary.
 * Otherwise, log an error message and return 0. */
int prepare_cache_dir(config_t *cfg, request_rec *r)
{
  apr_finfo_t finfo;    /* File information for the cache directory */
  apr_status_t status;  /* Return value from a file operation */

  /* If the cache directory wasn't specified in the configuration file, use a
   * default directory. */
  if (cfg->cache_dir == NULL) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "A Gosp cache directory was not specified in the Apache configuration file using the GospCacheDir directive");
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
		 "Using %s as the cache directory", DEFAULT_CACHE_DIR);
    cfg->cache_dir = DEFAULT_CACHE_DIR;
  }

  /* Ensure that the cache directory exists and is a directory. */
  status = apr_stat(&finfo, cfg->cache_dir, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_NOTICE, status, r->server,
                 "Failed to query cache directory %s; creating it", cfg->cache_dir);
    status =
      apr_dir_make_recursive(cfg->cache_dir,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to create cache directory %s", cfg->cache_dir);
    status = apr_stat(&finfo, cfg->cache_dir, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to query cache directory %s", cfg->cache_dir);
  }
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(APLOG_ALERT, APR_SUCCESS,
                 "Gosp cache directory %s is not a directory", cfg->cache_dir);

  /* Everything is okay. */
  return 1;
}
