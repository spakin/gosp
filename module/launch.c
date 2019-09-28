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

/* Use gosp2go to compile a Go Server Page into an executable program. */
gosp_status_t compile_gosp_server(request_rec *r, const char *server_name)
{
  apr_procattr_t *attr;             /* Process attributes */
  apr_proc_t proc;                  /* Launched process */
  const char **args;                /* Process command-line arguments */
  const char **envp;                /* Process environment */
  char *go_cache;                   /* Directory for the Go build cache */
  gosp_server_config_t *sconfig;    /* Server configuration */
  gosp_context_config_t *cconfig;   /* Context configuration */
  const char *work_dir;             /* Top-level work directory */
  const char *imports;              /* List of allowed imports */
  apr_status_t status;              /* Status of an APR call */

  /* Announce what we're about to do. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, APR_SUCCESS, r,
                "Compiling %s into %s",
                r->filename, server_name);

  /* Ensure we have a place to write the executable. */
  if (create_directories_for(r->server, r->pool, server_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Prepare a Go build cache. */
  sconfig = ap_get_module_config(r->server->module_config, &gosp_module);
  work_dir = sconfig->work_dir;
  go_cache = concatenate_filepaths(r->server, r->pool, work_dir, "go-build", NULL);
  if (go_cache == NULL)
    return GOSP_STATUS_FAIL;
  LAUNCH_CALL(apr_env_set("GOCACHE", go_cache, r->pool),
              "Setting GOCACHE=%s failed", go_cache);

  /* Prepare the process attributes. */
  LAUNCH_CALL(apr_procattr_create(&attr, r->pool),
              "Creating " GOSP2GO " process attributes failed");
  LAUNCH_CALL(apr_procattr_error_check_set(attr, 1),
              "Specifying that there should be extra error checking for " GOSP2GO);
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM),
              "Specifying that " GOSP2GO " should not inherit its parent's environment");

  /* Establish an environment for the child. */
  cconfig = (gosp_context_config_t *) ap_get_module_config(r->per_dir_config, &gosp_module);
  if (cconfig->go_path == NULL)
    envp = append_string(r->pool, (const char **) environ,
                         apr_pstrcat(r->pool, "GOPATH=", DEFAULT_GO_PATH, NULL));
  else
    envp = append_string(r->pool, (const char **) environ,
                         apr_pstrcat(r->pool, "GOPATH=", cconfig->go_path, NULL));

  /* Acquire a list of allowed package imports.  If not specified, don't allow
   * any package to be imported (except gosp, which is always allowed. */
  imports = cconfig->allowed_imports == NULL ? "NONE" : cconfig->allowed_imports;
  if (imports[0] == '+')  /* "+" is not meaningful at this point in the execution. */
    imports++;

  /* Spawn the gosp2go process and wait for it to complete. */
  args = (const char **) apr_palloc(r->pool, 12*sizeof(char *));
  args[0] = GOSP2GO;
  args[1] = "--build";
  args[2] = "-o";
  args[3] = server_name;
  args[4] = "--go";
  args[5] = cconfig->go_cmd;
  args[6] = "--allowed";
  args[7] = imports;
  args[8] = "--max-top";
  args[9] = cconfig->max_top == NULL ? "1000000000" : cconfig->max_top;
  args[10] = r->filename;
  args[11] = NULL;
  status = apr_proc_create(&proc, args[0], args, envp, attr, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                         "Failed to run " GOSP2GO);
  if (await_process_completion(r, &proc, GOSP2GO) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Launch a Go Server Page process to handle the current page.  Return
 * GOSP_STATUS_OK on success, GOSP_STATUS_NEED_ACTION if the executable wasn't
 * found (and presumably needs to be built), and GOSP_STATUS_FAIL if an
 * unexpected error occurred (and the request needs to be aborted). */
gosp_status_t launch_gosp_process(request_rec *r, const char *server_name, const char *sock_name)
{
  apr_proc_t proc;                  /* Launched process */
  apr_procattr_t *attr;             /* Process attributes */
  const char **envp;                /* Process environment */
  const char **args;                /* Process command-line arguments */
  gosp_context_config_t *cconfig;   /* Context configuration */
  apr_status_t status;              /* Status of an APR call */
  int i;

  /* Announce what we're about to do. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, APR_SUCCESS, r,
                "Launching Gosp process %s", server_name);

  /* Ensure we have a place to write the socket. */
  if (create_directories_for(r->server, r->pool, sock_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Prepare the process attributes. */
  LAUNCH_CALL(apr_procattr_create(&attr, r->pool),
              "Creating Gosp process attributes failed");
  LAUNCH_CALL(apr_procattr_detach_set(attr, 1),
              "Specifying that a Gosp process should detach from its parent");
  LAUNCH_CALL(apr_procattr_error_check_set(attr, 1),
              "Specifying that there should be extra error checking for a Gosp process");
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM),
              "Specifying that a Gosp process should not inherit its parent's environment");

  /* Establish an environment for the child. */
  cconfig = (gosp_context_config_t *) ap_get_module_config(r->per_dir_config, &gosp_module);
  if (cconfig->go_path == NULL)
    envp = (const char **) environ;
  else
    envp = append_string(r->pool, (const char **) environ,
                         apr_pstrcat(r->pool, "GOPATH=", cconfig->go_path, NULL));

  /* Spawn the Gosp process.  Even though the "detatch" attribute is set, it
   * appears that we need to await its completion to avoid leaving defunct
   * processes lying around. */
  args = (const char **) apr_palloc(r->pool, 8*sizeof(char *));
  i = 0;
  args[i++] = cconfig->gosp_server;
  args[i++] = "-plugin";
  args[i++] = server_name;
  args[i++] = "-socket";
  args[i++] = sock_name;
  if (cconfig->max_idle != NULL) {
    args[i++] = "-max-idle";
    args[i++] = cconfig->max_idle;
  }
  args[i++] = NULL;
  status = apr_proc_create(&proc, args[0], args, envp, attr, r->pool);
  if (status != APR_SUCCESS) {
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                         "Failed to run %s with plugin %s",
                         cconfig->gosp_server, server_name);
  }
  if (await_process_completion(r, &proc, server_name) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Kill a running Gosp server.  Return GOSP_STATUS_OK if the Gosp server is no
 * longer running, GOSP_STATUS_FAIL if it might be. */
gosp_status_t kill_gosp_server(request_rec *r, const char *sock_name, const char *server_name)
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
  status = apr_file_remove(server_name, r->pool);
  if (status != APR_SUCCESS && !APR_STATUS_IS_ENOENT(status)) {
    ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                  "Failed to remove Gosp server %s", server_name);
    return GOSP_STATUS_FAIL;
  }
  return GOSP_STATUS_OK;
}
