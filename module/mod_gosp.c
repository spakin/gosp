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
  gosp_config_t *config = cfg; /* Server configuration */
  config->work_dir = apr_pstrdup(cmd->pool, arg);
  return NULL;
}

/* Map a user name to a user ID. */
const char *gosp_set_user_id(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_config_t *config = cfg; /* Server configuration */
  apr_uid_t user_id;           /* User ID encountered */
  apr_gid_t group_id;          /* Group ID encountered */
  apr_status_t status;         /* Status of an APR call */

  if (arg[0] == '#') {
    /* Hash followed by a user ID: convert the ID from a string to an
     * integer. */
    config->user_id = (apr_uid_t) apr_atoi64(arg + 1);
  }
  else {
    /* User name: look up the corresponding user ID. */
    status = apr_uid_get(&user_id, &group_id, arg, cmd->temp_pool);
    if (status != APR_SUCCESS)
      return "Failed to map configuration option User to a user ID";
    config->user_id = user_id;
  }
  return NULL;
}

/* Map a group name to a group ID. */
const char *gosp_set_group_id(cmd_parms *cmd, void *cfg, const char *arg)
{
  gosp_config_t *config = cfg; /* Server configuration */
  apr_gid_t group_id;          /* Group ID encountered */
  apr_status_t status;         /* Status of an APR call */

  if (arg[0] == '#') {
    /* Hash followed by a group ID: convert the ID from a string to an
     * integer. */
    config->group_id = (apr_uid_t) apr_atoi64(arg + 1);
  }
  else {
    /* Group name: look up the corresponding group ID. */
    status = apr_gid_get(&group_id, arg, cmd->temp_pool);
    if (status != APR_SUCCESS)
      return "Failed to map configuration option Group to a group ID";
    config->group_id = group_id;
  }
  return NULL;
}

/* Define all of the configuration-file directives we accept. */
static const command_rec gosp_directives[] =
  {
   AP_INIT_TAKE1("GospWorkDir", gosp_set_work_dir, NULL, RSRC_CONF|ACCESS_CONF,
                 "Name of a directory in which Gosp can generate files needed during execution"),
   AP_INIT_TAKE1("User", gosp_set_user_id, NULL, RSRC_CONF|ACCESS_CONF,
                 "The user under which the server will answer requests"),
   AP_INIT_TAKE1("Group", gosp_set_group_id, NULL, RSRC_CONF|ACCESS_CONF,
                 "The group under which the server will answer requests"),
   { NULL }
  };

/* Allocate and initialize a configuration data structure. */
static void *gosp_allocate_dir_config(apr_pool_t *p, char *path)
{
  gosp_config_t *config;

  config = apr_palloc(p, sizeof(gosp_config_t));
  config->work_dir = ap_server_root_relative(p, DEFAULT_WORK_DIR);
  config->user_id = 0;
  config->group_id = 0;
  return (void *) config;
}

/* Run after the configuration file has been processed but before lowering
 * privileges. */
static int gosp_post_config(apr_pool_t *pconf, apr_pool_t *plog,
                            apr_pool_t *ptemp, server_rec *s)
{
  /* TODO: Create a lock file. */
  return OK;
}

/* Handle requests of type "gosp" by passing them to the gosp2go tool. */
static int gosp_handler(request_rec *r)
{
  apr_status_t status;       /* Status of an APR call */
  int launch_status;         /* Status returned by launch_gosp_process() */
  char *sock_name;           /* Name of the Unix-domain socket to connect to */
  apr_socket_t *sock;        /* The Unix-domain socket proper */
  apr_finfo_t finfo;         /* File information for the rquested file */
  char *lock_name;           /* Name of a lock file associated with the request */
  apr_global_mutex_t *lock;  /* Lock associated with the request */
  gosp_config_t *config;     /* Server configuration */

  /* We care only about "gosp" requests, and we don't care about HEAD
   * requests. */
  if (strcmp(r->handler, "gosp"))
    return DECLINED;
  if (r->header_only)
    return DECLINED;

  /* Issue an HTTP error if the requested Gosp file doesn't exist. */
  status = apr_stat(&finfo, r->canonical_filename, 0, r->pool);
  if (status != APR_SUCCESS)
    return HTTP_NOT_FOUND;

  /* Acquire access to our configuration information. */
  config = ap_get_module_config(r->per_dir_config, &gosp_module);

  /* Create and prepare the cache work directory.  Although it would be nice to
   * hoist this into the post-config handler, that runs before switching users
   * while gosp_handler runs after switching users.  Creating a directory in a
   * post-config handler would therefore lead to permission-denied errors. */
  ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
               "Using %s as Gosp's work directory", config->work_dir);
  if (create_directories_for(r, config->work_dir, 1) != GOSP_STATUS_OK)
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
  ap_hook_post_config(gosp_post_config, NULL, NULL, APR_HOOK_REALLY_FIRST);
  ap_hook_handler(gosp_handler, NULL, NULL, APR_HOOK_LAST);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA gosp_module =
  {
   STANDARD20_MODULE_STUFF,
   gosp_allocate_dir_config,  /* Allocate per-server configuration */
   NULL,                      /* Merge per-server configurations */
   NULL,                      /* Allocate per-server configuration */
   NULL,                      /* Merge per-server configurations */
   gosp_directives,           /* Define our configuration directives */
   gosp_register_hooks        /* Register Gosp hooks */
  };
