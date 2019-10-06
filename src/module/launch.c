/***********************************************
 * Launch a Gosp server from the Apache module *
 *                                             *
 * By Scott Pakin <scott+gosp@pakin.org>       *
 **********************************************/

#include "gosp.h"

/* Invoke an APR call as part of process launch.  On failure, log an error
 * message and return GOSP_STATUS_FAIL. */
#define LAUNCH_CALL(FCALL, ...)                                         \
  do {                                                                  \
    status = FCALL;                                                     \
    if (status != APR_SUCCESS) {                                        \
      ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r, __VA_ARGS__);     \
      return GOSP_STATUS_FAIL;                                          \
    }                                                                   \
  } while (0)

extern char **environ;   /* Process environment passed to the Apache module */

/* Wait for a process to complete. */
static gosp_status_t await_process_completion(request_rec *r, apr_proc_t *proc, const char *proc_name)
{
  int exit_code;              /* gosp2go return code */
  apr_exit_why_e exit_why;    /* Condition under which gosp2go exited */
  apr_status_t status;        /* Status of an APR call */

  status = apr_proc_wait(proc, &exit_code, &exit_why, APR_WAIT);
  if (status != APR_CHILD_DONE)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "%s was supposed to finish but didn't", proc_name);
  if (exit_why != APR_PROC_EXIT)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "Abnormal exit from %s", proc_name);
  if (exit_code != 0 && exit_code != APR_ENOTIMPL)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "Nonzero exit code (%d) from %s",
                         exit_code, proc_name);
  return GOSP_STATUS_OK;
}

/* Log a NULL-terminated command line to the log file. */
static void log_command_line(request_rec *r, const char **args)
{
  char *msg = "Command line:";
  const char **a;

  for (a = args; *a != NULL; a++)
    msg = apr_pstrcat(r->pool, msg, " ", *a, NULL);
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, APR_SUCCESS, r, "%s", msg);
}

/* Launch a process and wait for it to complete. */
static gosp_status_t launch_and_wait(request_rec *r, const char **args, int detach)
{
  apr_proc_t proc;                  /* Launched process */
  const char **envp;                /* Process environment */
  apr_procattr_t *attr;             /* Process attributes */
  gosp_context_config_t *cconfig;   /* Context configuration */
  apr_status_t status;              /* Status of an APR call */

  /* Prepare the process attributes. */
  LAUNCH_CALL(apr_procattr_create(&attr, r->pool),
              "Creating %s process attributes failed", args[0]);
  LAUNCH_CALL(apr_procattr_error_check_set(attr, 1),
              "Specifying that there should be extra error checking for a %s process", args[0]);
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM),
              "Specifying that a %s process should not inherit its parent's environment", args[0]);
  if (detach)
    LAUNCH_CALL(apr_procattr_detach_set(attr, 1),
                "Specifying that a %s process should detach from its parent", args[0]);

  /* Establish an environment for the child. */
  cconfig = (gosp_context_config_t *) ap_get_module_config(r->per_dir_config, &gosp_module);
  if (cconfig->go_path == NULL)
    envp = append_string(r->pool, (const char **) environ,
                         apr_pstrcat(r->pool, "GOPATH=", DEFAULT_GO_PATH, NULL));
  else
    envp = append_string(r->pool, (const char **) environ,
                         apr_pstrcat(r->pool, "GOPATH=", cconfig->go_path, NULL));

  /* Spawn the process and wait for it to complete.  It appears we need to do
   * this even for detached process to avoid leaving defunct processes lying
   * around. */
  log_command_line(r, args);
  status = apr_proc_create(&proc, args[0], args, envp, attr, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                         "Failed to run %s", args[0]);
  if (await_process_completion(r, &proc, args[0]) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Use gosp2go to compile a Go Server Page into a plugin. */
gosp_status_t compile_gosp_server(request_rec *r, const char *plugin_name)
{
  const char **args;                /* Process command-line arguments */
  char *go_cache;                   /* Directory for the Go build cache */
  gosp_server_config_t *sconfig;    /* Server configuration */
  gosp_context_config_t *cconfig;   /* Context configuration */
  const char *work_dir;             /* Top-level work directory */
  const char *imports;              /* List of allowed imports */
  apr_status_t status;              /* Status of an APR call */

  /* Announce what we're about to do. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, APR_SUCCESS, r,
                "Compiling %s into %s",
                r->filename, plugin_name);

  /* Ensure we have a place to write the executable. */
  if (create_directories_for(r->server, r->pool, plugin_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Prepare a Go build cache. */
  sconfig = ap_get_module_config(r->server->module_config, &gosp_module);
  work_dir = sconfig->work_dir;
  go_cache = concatenate_filepaths(r->server, r->pool, work_dir, "go-build", NULL);
  if (go_cache == NULL)
    return GOSP_STATUS_FAIL;
  LAUNCH_CALL(apr_env_set("GOCACHE", go_cache, r->pool),
              "Setting GOCACHE=%s failed", go_cache);

  /* Acquire a list of allowed package imports.  If not specified, don't allow
   * any package to be imported (except gosp, which is always allowed. */
  cconfig = (gosp_context_config_t *) ap_get_module_config(r->per_dir_config, &gosp_module);
  imports = cconfig->allowed_imports == NULL ? "NONE" : cconfig->allowed_imports;
  if (imports[0] == '+')  /* "+" is not meaningful at this point in the execution. */
    imports++;

  /* Construct the argument list. */
  args = (const char **) apr_palloc(r->pool, 12*sizeof(char *));
  args[0] = GOSP2GO;
  args[1] = "--build";
  args[2] = "-o";
  args[3] = plugin_name;
  args[4] = "--go";
  args[5] = cconfig->go_cmd;
  args[6] = "--allowed";
  args[7] = imports;
  args[8] = "--max-top";
  args[9] = cconfig->max_top == NULL ? "1000000000" : cconfig->max_top;
  args[10] = r->filename;
  args[11] = NULL;

  /* Spawn gosp2go and wait for it to complete. */
  return launch_and_wait(r, args, FALSE);
}

/* Launch a Go Server Page process to handle the current page.  Return
 * GOSP_STATUS_OK on success, GOSP_STATUS_NEED_ACTION if the plugin wasn't
 * found (and presumably needs to be built), and GOSP_STATUS_FAIL if an
 * unexpected error occurred (and the request needs to be aborted). */
gosp_status_t launch_gosp_server(request_rec *r, const char *plugin_name, const char *sock_name)
{
  const char **args;                /* Process command-line arguments */
  gosp_context_config_t *cconfig;   /* Context configuration */
  int i;

  /* Announce what we're about to do. */
  cconfig = (gosp_context_config_t *) ap_get_module_config(r->per_dir_config, &gosp_module);
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, APR_SUCCESS, r,
                "Launching %s to serve %s", cconfig->gosp_server, plugin_name);

  /* Ensure we have a place to write the socket. */
  if (create_directories_for(r->server, r->pool, sock_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Construct the argument list. */
  args = (const char **) apr_palloc(r->pool, 9*sizeof(char *));
  i = 0;
  args[i++] = cconfig->gosp_server;
  args[i++] = "-plugin";
  args[i++] = plugin_name;
  args[i++] = "-socket";
  args[i++] = sock_name;
  if (cconfig->max_idle != NULL) {
    args[i++] = "-max-idle";
    args[i++] = cconfig->max_idle;
  }
  args[i++] = "-dry-run";  /* This is removed below. */
  args[i++] = NULL;

  /* Spawn gosp-server in the foreground with -dry-run.  This is so if it
   * fails we'll log a useful error message. */
  if (launch_and_wait(r, args, FALSE) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Spawn gosp-server in the background without -dry-run.  If it fails, we'll
   * have no good of knowing, but we hope to have caught all common errors with
   * the foreground launch. */
  i -= 2;
  args[i++] = NULL;
  return launch_and_wait(r, args, TRUE);
}

/* Kill a running Gosp server.  Return GOSP_STATUS_OK if the Gosp server is no
 * longer running, GOSP_STATUS_FAIL if it might be. */
gosp_status_t kill_gosp_server(request_rec *r, const char *sock_name, const char *plugin_name)
{
  apr_status_t status;        /* Status of an APR call */

  /* Ask the server to shut down cleanly. */
  (void) send_termination_request(r, sock_name);

  /* Remove the socket. */
  status = apr_file_remove(sock_name, r->pool);
  if (status != APR_SUCCESS && !APR_STATUS_IS_ENOENT(status)) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                  "Failed to remove socket %s", sock_name);
    return GOSP_STATUS_FAIL;
  }

  /* Remove the server executable. */
  status = apr_file_remove(plugin_name, r->pool);
  if (status != APR_SUCCESS && !APR_STATUS_IS_ENOENT(status)) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                  "Failed to remove Gosp plugin %s", plugin_name);
    return GOSP_STATUS_FAIL;
  }
  return GOSP_STATUS_OK;
}
