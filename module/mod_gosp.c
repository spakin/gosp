/*****************************************
 * Apache module for Go Server Pages     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Define our configuration options. */
static config_t config;

/* Assign a value to the cache directory. */
const char *gosp_set_cache_dir(cmd_parms *cmd, void *cfg, const char *arg)
{
  config.cache_dir = arg;
  return NULL;
}

/* Define all of the configuration-file directives we accept. */
static const command_rec gosp_directives[] =
  {
   AP_INIT_TAKE1("GospCacheDir", gosp_set_cache_dir, NULL, RSRC_CONF,
                 "Name of a directory in which to cache generated files"),
   { NULL }
  };

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
  if (!prepare_cache_dir(&config, r))
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
