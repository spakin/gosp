/*****************************************
 * Apache module for Go Server Pages     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"

/* Handle requests of type "gosp" by passing them to the gosp2go tool. */
static int gosp_handler(request_rec *r)
{
  /* We care only about "gosp" requests, and we don't care about HEAD
   * requests. */
  if (strcmp(r->handler, "gosp"))
    return DECLINED;
  if (r->header_only)
    return DECLINED;

  /* Go Server Pages are always expressed in HTML. */
  r->content_type = "text/html";      

  /* Temporary placeholder */
  ap_rputs("The sample page from mod_gosp.c\n", r);
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
   NULL,                /* Any directives we may have for httpd */
   gosp_register_hooks  /* Register Gosp hooks */
  };
