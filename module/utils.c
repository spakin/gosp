/*****************************************
 * Various utility functions for use by  *
 * the Go Server Pages Apache module     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Return GOSP_STATUS_OK if a given directory, named in the Apache
 * configuration file, is valid, creating it if necessary.  Otherwise, log an
 * error message and return GOSP_STATUS_FAIL.  If the directory name is NULL,
 * assign it a default value. */
gosp_status_t prepare_config_directory(request_rec *r, const char *dir_type,
				       const char **dir_name, const char *default_name,
				       const char *config_name)
{
  apr_finfo_t finfo;    /* File information for the directory */
  apr_status_t status;  /* Return value from a file operation */

  /* If the directory wasn't specified in the configuration file, use a default
   * directory. */
  if (*dir_name == NULL) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "The Apache configuration file does not specify %s", config_name);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, APR_SUCCESS, r->server,
                 "Using %s as Gosp's %s directory", default_name, dir_type);
    *dir_name = default_name;
  }

  /* Ensure that the directory exists and is a directory. */
  status = apr_stat(&finfo, *dir_name, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_NOTICE, status, r->server,
                 "Failed to query %s directory %s; creating it", dir_type, *dir_name);
    status =
      apr_dir_make_recursive(*dir_name,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                   "Failed to create %s directory %s", dir_type, *dir_name);
    status = apr_stat(&finfo, *dir_name, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                   "Failed to query %s directory %s", dir_type, *dir_name);
  }
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Gosp %s directory %s is not a directory", dir_type, *dir_name);

  /* Everything is okay. */
  return GOSP_STATUS_OK;
}

/* Create a directory hierarchy in which to store the given file.
 * Return an APR status code. */
apr_status_t create_directories_for(request_rec *r, const char *fname)
{
  char *dir_name;       /* Directory containing fname */
  apr_finfo_t finfo;    /* File information for the directory */
  apr_status_t status;  /* Return value from an APR operation */

  /* Check if the directory exists and is a directory. */
  dir_name = dirname(apr_pstrdup(r->pool, fname));
  status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, r->pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, status, r->server,
                 "Failed to query directory %s; creating it", dir_name);
    status =
      apr_dir_make_recursive(dir_name,
                             APR_FPROT_UREAD|APR_FPROT_UWRITE|APR_FPROT_UEXECUTE,
                             r->pool);
    if (status != APR_SUCCESS)
      return status;
    status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, r->pool);
    if (status != APR_SUCCESS)
      return status;
  }
  if (finfo.filetype != APR_DIR)
    return APR_ENOTDIR;

  /* Everything is okay. */
  return APR_SUCCESS;
}

/* Securely concatenate two or more filepaths.  The final entry must be NULL.
 * Return the combined path name or NULL on error. */
char *concatenate_filepaths(request_rec *r, ...)
{
  va_list ap;           /* Argument pointer */
  char *merged_name;    /* Path name consisting of pathA followed by pathB */
  char *next_path;      /* Next filepath component */
  apr_status_t status;  /* Status of an APR call */

  /* Process the first argument. */
  va_start(ap, r);
  merged_name = va_arg(ap, char *);
  if (merged_name == NULL)
    return NULL;

  /* Process all remaining arguments. */
  while (1) {
    next_path = va_arg(ap, char *);
    if (next_path == NULL)
      return merged_name;
    if (next_path[0] == '/')
      next_path++;   /* APR_FILEPATH_SECUREROOT won't allow merging to an absolute pathname. */
    status = apr_filepath_merge(&merged_name, merged_name, next_path,
                                APR_FILEPATH_SECUREROOT|APR_FILEPATH_NOTRELATIVE,
                                r->pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(NULL, APLOG_ALERT, status,
                   "Failed to securely merge %s and %s", merged_name, next_path);
  }
}

/* Return 1 if the first file named is newer than the second or the second
 * doesn't exist.  It is assumed that the first first exists.  Return -1 on
 * error.*/
int is_newer_than(request_rec *r, const char *first, const char *second)
{
  apr_finfo_t finfo1;   /* File information for the first file */
  apr_finfo_t finfo2;   /* File information for the second file */
  apr_status_t status;  /* Status of an APR call */

  /* Query the second file's metadata. */
  status = apr_stat(&finfo2, second, APR_FINFO_MTIME, r->pool);
  if (APR_STATUS_IS_ENOENT(status))
    return 1;
  if (status != APR_SUCCESS)
    return -1;

  /* Query the first file's metadata. */
  status = apr_stat(&finfo1, first, APR_FINFO_MTIME, r->pool);
  if (status != APR_SUCCESS)
    return -1;

  /* Return 1 if the first file is newer, 0 otherwise. */
  return finfo1.mtime > finfo2.mtime;
}
