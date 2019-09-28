/*****************************************
 * Apache module for Go Server Pages     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Forward-declare our module. */
module AP_MODULE_DECLARE_DATA gosp_module;

/* Assign a value to the work directory. */
const char *gosp_set_work_dir(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_server_config_t *sconfig;    /* Server configuration */
  sconfig = ap_get_module_config(cmd->server->module_config, &gosp_module);
  sconfig->work_dir = ap_server_root_relative(cmd->pool, arg);
  return NULL;
}

/* Assign a value to the GOPATH environment variable. */
const char *gosp_set_go_path(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->go_path = arg;
  return NULL;
}

/* Assign the name of the Go compiler. */
const char *gosp_set_go_compiler(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->go_cmd = arg;
  return NULL;
}

/* Assign the name of the gosp-server command. */
const char *gosp_set_gosp_server(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->gosp_server = arg;
  return NULL;
}

/* Assign the maximum number of minutes a Gosp server is allowed to be idle. */
const char *gosp_set_max_idle(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->max_idle = arg;
  return NULL;
}

/* Assign the maximum number of ?go:top blocks allowed on a single page. */
const char *gosp_set_max_top(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->max_top = arg;
  return NULL;
}

/* Assign the set of packages a Go Server Page is allowed to import. */
const char *gosp_set_allowed_imports(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_context_config_t *cconfig;   /* Per-context configuration */
  cconfig = (gosp_context_config_t *) cfg;
  cconfig->allowed_imports = arg;
  return NULL;
}

/* Map a user name to a user ID. */
const char *gosp_set_user_id(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_server_config_t *sconfig;      /* Server configuration */
  apr_uid_t user_id;                  /* User ID encountered */
  apr_gid_t group_id;                 /* Group ID encountered */
  apr_status_t status;                /* Status of an APR call */

  sconfig = ap_get_module_config(cmd->server->module_config, &gosp_module);
  if (arg[0] == '#') {
    /* Hash followed by a user ID: convert the ID from a string to an
     * integer. */
    sconfig->user_id = (apr_uid_t) apr_atoi64(arg + 1);
  }
  else {
    /* User name: look up the corresponding user ID. */
    status = apr_uid_get(&user_id, &group_id, arg, cmd->temp_pool);
    if (status != APR_SUCCESS)
      return "Failed to map configuration option User to a user ID";
    sconfig->user_id = user_id;
  }
  return NULL;
}

/* Map a group name to a group ID. */
const char *gosp_set_group_id(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_server_config_t *sconfig;      /* Server configuration */
  apr_gid_t group_id;                 /* Group ID encountered */
  apr_status_t status;                /* Status of an APR call */

  sconfig = ap_get_module_config(cmd->server->module_config, &gosp_module);
  if (arg[0] == '#') {
    /* Hash followed by a group ID: convert the ID from a string to an
     * integer. */
    sconfig->group_id = (apr_uid_t) apr_atoi64(arg + 1);
  }
  else {
    /* Group name: look up the corresponding group ID. */
    status = apr_gid_get(&group_id, arg, cmd->temp_pool);
    if (status != APR_SUCCESS)
      return "Failed to map configuration option Group to a group ID";
    sconfig->group_id = group_id;
  }
  return NULL;
}

/* Define all of the configuration-file directives we accept. */
static const command_rec gosp_directives[] =
  {
   AP_INIT_TAKE1("GospWorkDir", gosp_set_work_dir, NULL, RSRC_CONF|ACCESS_CONF,
                 "Name of a directory in which Gosp can generate files needed during execution"),
   AP_INIT_TAKE1("GospGoPath", gosp_set_go_path, NULL, RSRC_CONF|ACCESS_CONF,
                 "Value of the GOPATH environment variable to use when building Gosp pages"),
   AP_INIT_TAKE1("GospGoCompiler", gosp_set_go_compiler, NULL, RSRC_CONF|ACCESS_CONF,
                 "Go compiler executable"),
   AP_INIT_TAKE1("GospServer", gosp_set_gosp_server, NULL, RSRC_CONF|ACCESS_CONF,
                 "gosp-server executable"),
   AP_INIT_TAKE1("GospMaxIdleTime", gosp_set_max_idle, NULL, RSRC_CONF|ACCESS_CONF,
                 "Maximum idle time before a Gosp server automatically exits"),
   AP_INIT_TAKE1("GospMaxTop", gosp_set_max_top, NULL, RSRC_CONF|ACCESS_CONF,
                 "Maximum number of top-level blocks allowed per page"),
   AP_INIT_TAKE1("GospAllowedImports", gosp_set_allowed_imports, NULL, RSRC_CONF|ACCESS_CONF,
                 "Comma-separated list of packages that can be imported or \"ALL\" or \"NONE\""),
   AP_INIT_TAKE1("User", gosp_set_user_id, NULL, RSRC_CONF|ACCESS_CONF,
                 "The user under which the server will answer requests"),
   AP_INIT_TAKE1("Group", gosp_set_group_id, NULL, RSRC_CONF|ACCESS_CONF,
                 "The group under which the server will answer requests"),
   { NULL }
  };

/* Allocate and initialize a per-server configuration. */
static void *gosp_allocate_server_config(apr_pool_t *p, server_rec *s)
{
  gosp_server_config_t *sconfig;

  sconfig = apr_pcalloc(p, sizeof(gosp_server_config_t));
  sconfig->work_dir = ap_server_root_relative(p, DEFAULT_WORK_DIR);
  return (void *) sconfig;
}

/* Allocate and initialize a per-context configuration. */
static void *gosp_allocate_context_config(apr_pool_t *p, char *ctx)
{
  gosp_context_config_t *cconfig;

  cconfig = apr_pcalloc(p, sizeof(gosp_context_config_t));
  cconfig->context = apr_pstrdup(p, ctx ? ctx : "[undefined context]");
  cconfig->go_cmd = DEFAULT_GO_COMMAND;
  cconfig->gosp_server = GOSP_SERVER;
  return (void *) cconfig;
}

/* For use in gosp_merge_context_config(), assign a field to the merged context
 * from the child if non-NULL, otherwise from the parent. */
#define MERGE_CHILD_OVER_PARENT(FIELD) \
  merged->FIELD = child->FIELD == NULL ? parent->FIELD : child->FIELD

/* Merge two per-context configurations into a new configuration. */
static void *gosp_merge_context_config(apr_pool_t *p, void *base, void *delta) {
  gosp_context_config_t *parent = (gosp_context_config_t *)base;
  gosp_context_config_t *child = (gosp_context_config_t *)delta;
  gosp_context_config_t *merged;
  char *merged_name;

  /* For most fields, simply let the child take precedence over the parent. */
  merged_name = apr_psprintf(p, "Merger of %s and %s", parent->context, child->context);
  merged = (gosp_context_config_t *) gosp_allocate_context_config(p, merged_name);
  MERGE_CHILD_OVER_PARENT(go_cmd);
  MERGE_CHILD_OVER_PARENT(gosp_server);
  MERGE_CHILD_OVER_PARENT(go_path);
  MERGE_CHILD_OVER_PARENT(max_idle);
  MERGE_CHILD_OVER_PARENT(max_top);

  /* Merge allowed_imports differently if it begins with a "+" or not. */
  if (child->allowed_imports == NULL || child->allowed_imports[0] != '+')
    /* Retain only the child's import list. */
    MERGE_CHILD_OVER_PARENT(allowed_imports);
  else {
    /* Append the child's import list to its parent's. */
    const char *p_imps = parent->allowed_imports == NULL ? "ALL" : parent->allowed_imports;
    merged->allowed_imports = apr_pstrcat(p, p_imps, ",",
                                          child->allowed_imports + 1, NULL);
  }
  return merged;
}

/* Run after the configuration file has been processed but before lowering
 * privileges. */
static int gosp_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                            apr_pool_t *ptemp, server_rec *s)
{
  gosp_server_config_t *sconfig;      /* Server configuration */
  apr_status_t status;                /* Status of an APR call */

  /* Create our work directory. */
  sconfig = ap_get_module_config(s->module_config, &gosp_module);
  ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, s,
               "Using %s as Gosp's work directory", sconfig->work_dir);
  if (create_directories_for(s, ptemp, sconfig->work_dir, 1) != GOSP_STATUS_OK)
    return HTTP_INTERNAL_SERVER_ERROR;

  /* Create a global lock.  Store the mutex structure and name of the
   * underlying file in our configuration structure. */
  sconfig->lock_name = concatenate_filepaths(s, pconf, sconfig->work_dir,
                                            "global.lock", NULL);
  if (sconfig->lock_name == NULL)
    return HTTP_INTERNAL_SERVER_ERROR;
#ifdef APR_LOCK_DEFAULT_TIMED
  status = apr_global_mutex_create(&sconfig->mutex, sconfig->lock_name,
                                   APR_LOCK_DEFAULT_TIMED, pconf);
#else
  status = apr_global_mutex_create(&sconfig->mutex, sconfig->lock_name,
                                   APR_LOCK_DEFAULT, pconf);
#endif
  if (status != APR_SUCCESS)
    REPORT_SERVER_ERROR(HTTP_INTERNAL_SERVER_ERROR, APLOG_ERR, status,
                        "Failed to create lock file %s", sconfig->lock_name);
#ifdef AP_NEED_SET_MUTEX_PERMS
  status = ap_unixd_set_global_mutex_perms(sconfig->mutex);
  if (status != APR_SUCCESS)
    REPORT_SERVER_ERROR(HTTP_INTERNAL_SERVER_ERROR, APLOG_ERR, status,
                        "Failed to set permissions on lock file %s", sconfig->lock_name);
#endif
  return OK;
}

/* Perform per-child initialization. */
static void gosp_child_init(apr_pool_t *pool, server_rec *s)
{
  gosp_server_config_t *sconfig;     /* Server configuration */
  apr_status_t status;               /* Status of an APR call */

  /* Reconnect to the global mutex. */
  sconfig = ap_get_module_config(s->module_config, &gosp_module);
  status = apr_global_mutex_child_init(&sconfig->mutex, sconfig->lock_name, pool);
  if (status != APR_SUCCESS)
    ap_log_error(APLOG_MARK, APLOG_ERR, status, s,
                 "Failed to reconnect to lock file %s", sconfig->lock_name);
}

/* Handle requests of type "gosp" by passing them to the gosp2go tool. */
static int gosp_handler(request_rec *r)
{
  apr_finfo_t finfo;               /* File information for the rquested file */
  char *sock_name;                 /* Name of the socket on which the Gosp server is listening */
  char *server_name;               /* Name of the Gosp server executable */
  apr_time_t begin_time;           /* Time at which we began waiting for the server to launch */
  gosp_server_config_t *sconfig;   /* Server configuration */
  apr_status_t status;             /* Status of an APR call */
  gosp_status_t gstatus;           /* Status of an internal Gosp call */

  /* We care only about "gosp" requests, and we don't care about HEAD
   * requests. */
  if (strcmp(r->handler, "gosp"))
    return DECLINED;
  if (r->header_only)
    return DECLINED;

  /* Issue an HTTP File Not Found (404) error if the requested Gosp file
   * doesn't exist. */
  status = apr_stat(&finfo, r->filename, 0, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(HTTP_NOT_FOUND, APLOG_INFO, status,
                         "Failed to query Gosp page %s", r->filename);

  /* Gain access to our configuration information. */
  sconfig = ap_get_module_config(r->server->module_config, &gosp_module);

  /* Identify the name of the socket to use to communicate with the Gosp
   * server. */
  sock_name = concatenate_filepaths(r->server, r->pool, sconfig->work_dir, "sockets", r->filename, NULL);
  if (sock_name == NULL)
    REPORT_REQUEST_ERROR(HTTP_INTERNAL_SERVER_ERROR, APLOG_ERR, APR_SUCCESS,
                         "Failed to construct a socket name");
  sock_name = apr_pstrcat(r->pool, sock_name, ".sock", NULL);

  /* Identify the name of the Gosp server executable. */
  server_name = concatenate_filepaths(r->server, r->pool, sconfig->work_dir, "pages",
                                      apr_pstrcat(r->pool, r->filename, ".so", NULL),
                                      NULL);
  if (server_name == NULL)
    REPORT_REQUEST_ERROR(HTTP_INTERNAL_SERVER_ERROR, APLOG_ERR, APR_SUCCESS,
                         "Failed to construct the name of the Gosp server");

  /* If the Gosp server is newer than the Gosp file (the common case) we simply
   * handle the request and return. */
  if (is_newer_than(r, r->filename, server_name) == 0) {
    gstatus = simple_request_response(r, sock_name);
    if (gstatus == GOSP_STATUS_OK)
      return r->status == HTTP_OK ? OK : r->status;
    if (gstatus == GOSP_STATUS_FAIL)
      return HTTP_INTERNAL_SERVER_ERROR;
  }

  /* The Gosp file is newer than the Gosp server.  We therefore have a lot of
   * work to do.  To ensure that only process does the work we first acquire
   * the global lock. */
  if (acquire_global_lock(r->server) != GOSP_STATUS_OK)
    return HTTP_INTERNAL_SERVER_ERROR;

  /* Check again if the Gosp server is newer than the Gosp file.  If so, then
   * another process must have performed all the work while we were waiting for
   * the lock.  In this case, release the lock and handle the request. */
  if (is_newer_than(r, r->filename, server_name) == 0) {
    if (release_global_lock(r->server) != GOSP_STATUS_OK)
      return HTTP_INTERNAL_SERVER_ERROR;
    gstatus = simple_request_response(r, sock_name);
    if (gstatus == GOSP_STATUS_OK)
      return r->status == HTTP_OK ? OK : r->status;
    if (gstatus == GOSP_STATUS_FAIL)
      return HTTP_INTERNAL_SERVER_ERROR;
  }

  /* Kill the Gosp server, and remove its socket and executable. */
  gstatus = kill_gosp_server(r, sock_name, server_name);
  if (gstatus != GOSP_STATUS_OK) {
    (void) release_global_lock(r->server);
    return HTTP_INTERNAL_SERVER_ERROR;
  }

  /* Check if the Gosp server executable already exists.  If not, compile it. */
  status = apr_stat(&finfo, server_name, 0, r->pool);
  if (status != APR_SUCCESS) {
    /* The Gosp server doesn't appear to have been compiled.  Compile it. */
    gstatus = compile_gosp_server(r, server_name);
    if (gstatus != GOSP_STATUS_OK) {
      (void) release_global_lock(r->server);
      return HTTP_INTERNAL_SERVER_ERROR;
    }
  }

  /* At this point, the Gosp server executable exists.  Launch it. */
  gstatus = launch_gosp_process(r, server_name, sock_name);
  if (gstatus != GOSP_STATUS_OK) {
    (void) release_global_lock(r->server);
    return HTTP_INTERNAL_SERVER_ERROR;
  }

  /* Try again to have the Gosp server handle the request.  Wait up to
   * GOSP_LAUNCH_WAIT_TIME for the executable to finish launching before giving
   * up.  We retain the lock because we don't want unrelated requests to the
   * same page to fail because they didn't know the server is in the process of
   * launching. */
  begin_time = apr_time_now();
  while (apr_time_now() - begin_time < GOSP_LAUNCH_WAIT_TIME) {
    /* Keep retrying while we wait for the server to launch. */
    gstatus = simple_request_response(r, sock_name);
    if (gstatus == GOSP_STATUS_OK)
      break;
    if (gstatus == GOSP_STATUS_FAIL) {
      (void) release_global_lock(r->server);
      return HTTP_INTERNAL_SERVER_ERROR;
    }
    apr_sleep(1000);
  }

  /* Release the lock and return. */
  gstatus = release_global_lock(r->server);
  if (gstatus != GOSP_STATUS_OK)
    return HTTP_INTERNAL_SERVER_ERROR;
  return r->status == HTTP_OK ? OK : r->status;
}

/* Invoke gosp_handler at the end of every request. */
static void gosp_register_hooks(apr_pool_t *p)
{
  ap_hook_post_config(gosp_post_config, NULL, NULL, APR_HOOK_LAST);
  ap_hook_child_init(gosp_child_init, NULL, NULL, APR_HOOK_LAST);
  ap_hook_handler(gosp_handler, NULL, NULL, APR_HOOK_LAST);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA gosp_module =
  {
   STANDARD20_MODULE_STUFF,
   gosp_allocate_context_config,  /* Allocate a per-context configuration */
   gosp_merge_context_config,     /* Merge per-context configurations */
   gosp_allocate_server_config,   /* Allocate a per-server configuration */
   NULL,                          /* Merge per-server configurations */
   gosp_directives,               /* Define our configuration directives */
   gosp_register_hooks            /* Register Gosp hooks */
  };
