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

/* Assign a value to the run directory. */
const char *gosp_set_run_dir(cmd_parms *cmd, void *cfg, const char *arg)
{
  config.run_dir = arg;
  return NULL;
}

/* Define all of the configuration-file directives we accept. */
static const command_rec gosp_directives[] =
  {
   AP_INIT_TAKE1("GospCacheDir", gosp_set_cache_dir, NULL, RSRC_CONF,
                 "Name of a directory in which to cache generated files"),
   AP_INIT_TAKE1("GospRunDir", gosp_set_run_dir, NULL, RSRC_CONF,
                 "Name of a directory in which to keep files needed only during server execution"),
   { NULL }
  };

/* Process configuration options. */
static int gosp_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                            apr_pool_t *ptemp, server_rec *s)
{
  /* Prepare the cache directory and the run directory. */
  if (!prepare_directory(s, pconf, "cache", &config.cache_dir, DEFAULT_CACHE_DIR))
    return HTTP_INTERNAL_SERVER_ERROR;
  if (!prepare_directory(s, pconf, "run", &config.run_dir, DEFAULT_RUN_DIR))
    return HTTP_INTERNAL_SERVER_ERROR;
  return OK;
}

/* Handle requests of type "gosp" by passing them to the gosp2go tool. */
static int gosp_handler(request_rec *r)
{
  apr_status_t status;    /* Status of an APR call */
  char *sock_name;        /* Name of the Unix-domain socket to connect to */
  apr_socket_t *sock;     /* The Unix-domain socket proper */

  /* We care only about "gosp" requests, and we don't care about HEAD
   * requests. */
  if (strcmp(r->handler, "gosp"))
    return DECLINED;
  if (r->header_only)
    return DECLINED;

  /* Connect to the process that handles the requested Go Server Page. */
  sock_name = get_socket_name(r, config.run_dir);
  if (sock_name == NULL)
    return HTTP_INTERNAL_SERVER_ERROR;
  ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, APR_SUCCESS, r->server,
               "Communicating with a Gosp server via socket %s", sock_name);
  status = connect_socket(&sock, r, sock_name);

  /* Temporary */
  ap_log_error(APLOG_MARK, APLOG_NOTICE, status, r->server,
               "Connecting to socket %s returned %d", sock_name, status);

  /* Go Server Pages are always expressed in HTML. */
  r->content_type = "text/html";

  /* Temporary placeholder */
  ap_rprintf(r, "Translating %s\n", r->filename);
  return OK;
}

/* Invoke gosp_handler at the end of every request. */
static void gosp_register_hooks(apr_pool_t *p)
{
  ap_hook_post_config(gosp_post_config, NULL, NULL, APR_HOOK_MIDDLE);
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
