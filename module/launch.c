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
      ap_log_rerror(APLOG_MARK, APLOG_ALERT, status, r, __VA_ARGS__);   \
      return GOSP_STATUS_FAIL;                                          \
    }                                                                   \
  } while (0)

/* Use gosp2go to compile a Go Server Page into an executable program. */
gosp_status_t compile_gosp_server(request_rec *r, const char *server_name)
{
  apr_procattr_t *attr;       /* Process attributes */
  apr_proc_t proc;            /* Launched process */
  const char **args;          /* Process command-line arguments */
  int exit_code;              /* gosp2go return code */
  apr_exit_why_e exit_why;    /* Condition under which gosp2go exited */
  char *go_cache;             /* Directory for the Go build cache */
  gosp_config_t *config;      /* Server configuration */
  const char *work_dir;       /* Top-level work directory */
  apr_status_t status;        /* Status of an APR call */

  /* Announce what we're about to do. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r,
                "Compiling %s into %s",
                r->canonical_filename, server_name);

  /* Ensure we have a place to write the executable. */
  if (create_directories_for(r->server, r->pool, server_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Prepare a Go build cache. */
  config = ap_get_module_config(r->server->module_config, &gosp_module);
  work_dir = config->work_dir;
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
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM_ENV),
              "Specifying that " GOSP2GO " should inherit its parent's environment");

  /* Spawn the gosp2go process. */
  args = (const char **) apr_palloc(r->pool, 6*sizeof(char *));
  args[0] = GOSP2GO;
  args[1] = "--build";
  args[2] = "-o";
  args[3] = server_name;
  args[4] = r->canonical_filename;
  args[5] = NULL;
  status = apr_proc_create(&proc, args[0], args, NULL, attr, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                         "Failed to run " GOSP2GO);

  /* Wait for gosp2go to finish its execution. */
  status = apr_proc_wait(&proc, &exit_code, &exit_why, APR_WAIT);
  if (status != APR_CHILD_DONE)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                         "%s was supposed to finish but didn't", GOSP2GO);
  if (exit_why != APR_PROC_EXIT)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                         "Abnormal exit from %s --build -o %s %s",
                         GOSP2GO, server_name, r->canonical_filename);
  if (exit_code != 0 && exit_code != APR_ENOTIMPL)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                         "Nonzero exit code (%d) from %s --build -o %s %s",
                         exit_code, GOSP2GO, server_name, r->canonical_filename);
  return GOSP_STATUS_OK;
}

/* Launch a Go Server Page process to handle the current page.  Return
 * GOSP_STATUS_OK on success, GOSP_STATUS_NEED_ACTION if the executable wasn't
 * found (and presumably needs to be built), and GOSP_STATUS_FAIL if an
 * unexpected error occurred (and the request needs to be aborted). */
gosp_status_t launch_gosp_process(request_rec *r, const char *server_name, const char *sock_name)
{
  apr_proc_t proc;            /* Launched process */
  apr_procattr_t *attr;       /* Process attributes */
  const char **args;          /* Process command-line arguments */
  apr_status_t status;        /* Status of an APR call */

  /* Announce what we're about to do. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r,
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
  LAUNCH_CALL(apr_procattr_cmdtype_set(attr, APR_PROGRAM_ENV),
              "Specifying that a Gosp process should inherit its parent's environment");

  /* Spawn the Gosp process. */
  args = (const char **) apr_palloc(r->pool, 4*sizeof(char *));
  args[0] = server_name;
  args[1] = "-socket";
  args[2] = sock_name;
  args[3] = NULL;
  status = apr_proc_create(&proc, server_name, args, NULL, attr, r->pool);
  if (status == APR_SUCCESS)
    return GOSP_STATUS_OK;
  if (APR_STATUS_IS_ENOENT(status))
    return GOSP_STATUS_NEED_ACTION;
  REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                       "Failed to run %s", server_name);
}

/* Kill a running Gosp server.  We assume this is called because the Gosp page
 * is newer than the Gosp server. */
gosp_status_t kill_gosp_server(request_rec *r, const char *sock_name, const char *server_name)
{
  gosp_status_t errcode = GOSP_STATUS_OK;  /* Error code to return */
  apr_status_t status;        /* Status of an APR call */
  gosp_status_t gstatus;      /* Status of an internal Gosp call */

  /* Work within a critical section to ensure the Gosp server is killed exactly
   * once. */
  if (acquire_global_lock(r->server) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* On any error, first release the lock. */
  do {
    /* Check that the server was not already recompiled by another process. */
    if (is_newer_than(r, r->canonical_filename, sock_name) == 1) {
      gstatus = send_termination_request(r, sock_name);
      if (gstatus != GOSP_STATUS_OK) {
        errcode = GOSP_STATUS_FAIL;
        break;
      }
    }

    /* Remove the socket. */
    status = apr_file_remove(sock_name, r->pool);
    if (status != APR_SUCCESS) {
      ap_log_rerror(APLOG_MARK, APLOG_ALERT, status, r,
                    "Failed to remove socket %s", sock_name);
      errcode = GOSP_STATUS_FAIL;
      break;
    }

    /* Remove the server executable. */
    status = apr_file_remove(server_name, r->pool);
    if (status != APR_SUCCESS) {
      ap_log_rerror(APLOG_MARK, APLOG_ALERT, status, r,
                    "Failed to remove Gosp server %s", server_name);
      errcode = GOSP_STATUS_FAIL;
      break;
    }
  }
  while (0);

  /* Release the lock and return. */
  if (release_global_lock(r->server) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  return errcode;
}
