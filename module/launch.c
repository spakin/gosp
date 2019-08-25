/***********************************************
 * Launch a Gosp server from the Apache module *
 *                                             *
 * By Scott Pakin <scott+gosp@pakin.org>       *
 **********************************************/

/*
 * TODO
 *
 * Flow should be as follows:
 *
 * 1) Get socket name.
 * 2) Try connecting to socket.
 *    If filename error,
 *  3) Create all subdirectories.
 *     Try connecting to socket.  Abort on failure.
 *    Else if failure/timeout,
 *  4) Relaunch server.
 *     If not found,
 *    6) Build server.  Abort on failure.
 *    7) Launch server.  Abort on failure.
 *  8) Try connecting to socket.  Abort on failure.
 */

#include "gosp.h"

/* Invoke an APR call as part of process launch.  On failure, log an error
 * message and return GOSP_STATUS_FAIL. */
#define LAUNCH_CALL(FCALL, ...)                                            \
do {                                                                       \
  status = FCALL;                                                          \
  if (status != APR_SUCCESS) {                                             \
    ap_log_error(APLOG_MARK, APLOG_ALERT, status, r->server, __VA_ARGS__); \
    return GOSP_STATUS_FAIL;                                               \
  }                                                                        \
} while (0)

/* Launch a Go Server Page process to handle the current page.  Return
 * GOSP_STATUS_OK on success, GOSP_STATUS_NEED_ACTION if the executable wasn't
 * found (and presumably need to be built), and GOSP_STATUS_FAIL if an
 * unexpected error occurred (and the request needs to be aborted). */
gosp_status_t launch_gosp_process(request_rec *r, const char *work_dir, const char *sock_name)
{
  char *server_name;      /* Name of the Gosp server executable */
  apr_proc_t proc;        /* Launched process */
  apr_procattr_t *attr;   /* Process attributes */
  const char **args;      /* Process command-line arguments */
  apr_status_t status;    /* Status of an APR call */

  /* Ensure we have a place to write the socket. */
  if (create_directories_for(r, sock_name, 0) != GOSP_STATUS_OK)
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

  /* Prefix the Gosp page name with the run directory to produce the name of
   * the Gosp server. */
  server_name = concatenate_filepaths(r->server, r->pool, work_dir, "bin",
                                      apr_pstrcat(r->pool, r->canonical_filename, ".exe", NULL),
                                      NULL);
  if (server_name == NULL)
    return GOSP_STATUS_FAIL;

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
  REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
               "Failed to run %s", server_name);
}

/* Use gosp2go to compile a Go Server Page into an executable program. */
int compile_gosp_server(request_rec *r, const char *work_dir)
{
  apr_procattr_t *attr;     /* Process attributes */
  char *server_name;        /* Gosp executable filename */
  apr_proc_t proc;          /* Launched process */
  const char **args;        /* Process command-line arguments */
  int exit_code;            /* gosp2go return code */
  apr_exit_why_e exit_why;  /* Condition under which gosp2go exited */
  char *go_cache;           /* Directory for the Go build cache */
  apr_status_t status;      /* Status of an APR call */

  /* Ensure we have a place to write the executable. */
  server_name = concatenate_filepaths(r->server, r->pool, work_dir, "bin",
                                   apr_pstrcat(r->pool, r->canonical_filename, ".exe", NULL),
                                   NULL);
  if (server_name == NULL)
    return GOSP_STATUS_FAIL;
  if (create_directories_for(r, server_name, 0) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;

  /* Prepare a Go build cache. */
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
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to run " GOSP2GO);

  /* Wait for gosp2go to finish its execution. */
  status = apr_proc_wait(&proc, &exit_code, &exit_why, APR_WAIT);
  if (status != APR_CHILD_DONE)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                 "%s was supposed to finish but didn't", GOSP2GO);
  if (exit_why != APR_PROC_EXIT)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                 "Abnormal exit from %s --build -o %s %s",
                 GOSP2GO, server_name, r->canonical_filename);
  if (exit_code != 0 && exit_code != APR_ENOTIMPL)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, APR_SUCCESS,
                 "Nonzero exit code (%d) from %s --build -o %s %s",
                 exit_code, GOSP2GO, server_name, r->canonical_filename);
  return GOSP_STATUS_OK;
}
