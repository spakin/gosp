/*****************************************
 * Apache module for Go Server Pages     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* For now, our only configuration option is the name of a cache directory. */
static const char *cache_dir = NULL;

/* Assign a value to the cache directory. */
const char *gosp_set_cache_dir(cmd_parms *cmd, void *cfg, const char *arg)
{
  cache_dir = arg;
  return NULL;
}

/* Define all of the configuration-file directives we accept. */
static const command_rec gosp_directives[] =
  {
   AP_INIT_TAKE1("GospCacheDir", gosp_set_cache_dir, NULL, RSRC_CONF,
                 "Name of a directory in which to cache generated files"),
   { NULL }
  };

/* Return 1 if the cache directory is valid.  Otherwise, log an error message
 * and return 0. */
static int validate_cache_dir(request_rec *r)
{
  apr_finfo_t finfo;    /* File information for the cache directory */
  apr_status_t status;  /* Return value from a file operation */

  /* Ensure that the cache directory was specified in the configuration file. */
  if (cache_dir == NULL)
    REPORT_ERROR(APLOG_ALERT, APR_SUCCESS,
                 "A Gosp cache directory must be specified in the Apache configuration file using the GospCacheDir directive");

  /* Ensure that the cache directory exists and is a directory. */
  status = apr_stat(&finfo, cache_dir, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_NOTICE, status, r->server,
                 "Failed to query cache directory %s; creating it", cache_dir);
    status =
      apr_dir_make_recursive(cache_dir,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to create cache directory %s", cache_dir);
    status = apr_stat(&finfo, cache_dir, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(APLOG_ALERT, status,
                   "Failed to query cache directory %s", cache_dir);
  }
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(APLOG_ALERT, APR_SUCCESS,
                 "Gosp cache directory %s is not a directory", cache_dir);

  /* Everything is okay. */
  return 1;
}

/* Handle requests of type "gosp" by passing them to the gosp2go tool. */
static int gosp_handler(request_rec *r)
{
  /* We care only about "gosp" requests, and we don't care about HEAD
   * requests. */
  if (strcmp(r->handler, "gosp"))
    return DECLINED;
  if (r->header_only)
    return DECLINED;

  /* Complain if the cache directory is no good. */
  if (!validate_cache_dir(r))
    return HTTP_INTERNAL_SERVER_ERROR;

  /* Go Server Pages are always expressed in HTML. */
  r->content_type = "text/html";

  /* Temporary placeholder */
  ap_rprintf(r, "Translating %s\n", r->filename);
  return OK;
}

/* Invoke gosp_handler at the end of every request. */
static void gosp_register_hooks(apr_pool_t *p)
{
  ap_hook_handler(gosp_handler, NULL, NULL, APR_HOOK_LAST);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA gosp_module =
  {
   STANDARD20_MODULE_STUFF,
   NULL,                /* Per-directory configuration handler */
   NULL,                /* Merge handler for per-directory configurations */
   NULL,                /* Per-server configuration handler */
   NULL,                /* Merge handler for per-server configurations */
   gosp_directives,     /* Any directives we may have for httpd */
   gosp_register_hooks  /* Register Gosp hooks */
  };
