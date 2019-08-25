/*****************************************
 * Various utility functions for use by  *
 * the Go Server Pages Apache module     *
 *                                       *
 * By Scott Pakin <scott+gosp@pakin.org> *
 *****************************************/

#include "gosp.h"

/* Create a directory hierarchy in which to store the given file.  If is_dir =
 * 1, the last component of the file path is itself a directory. */
gosp_status_t create_directories_for(server_rec *s, apr_pool_t *pool, const char *fname, int is_dir)
{
  const char *dir_name;       /* Directory containing fname */
  apr_finfo_t finfo;          /* File information for the directory */
  gosp_config_t *config;      /* Server configuration */
  apr_status_t status;        /* Return value from an APR operation */

  /* Check if the directory exists.  If it doesn't exist, create it. */
  if (is_dir)
    dir_name = fname;
  else
    dir_name = dirname(apr_pstrdup(pool, fname));
  status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, pool);
  if (status != APR_SUCCESS) {
    /* If the directory couldn't be stat'ed, try creating it then stat'ing it
     * again. */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, status, s,
                 "Failed to query directory %s; creating it", dir_name);
    status =
      apr_dir_make_recursive(dir_name, GOSP_DIR_PERMS, pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                   "Failed to create directory %s", dir_name);
    status = apr_stat(&finfo, dir_name, APR_FINFO_TYPE, pool);
    if (status != APR_SUCCESS)
      REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                   "Failed to query directory %s", dir_name);
  }

  /* The directory exists.  Ensure that it really is a directory. */
  if (finfo.filetype != APR_DIR)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to create directory %s because it already exists as a non-directory", dir_name);

  /* Set the permissions to those with which requests are handled. */
  config = ap_get_module_config(s->module_config, &gosp_module);
  if (chown(dir_name, (uid_t)config->user_id, (gid_t)config->group_id) == -1) {
    status = APR_FROM_OS_ERROR(errno);
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to change ownership of directory, %s", dir_name);
  }

  /* Everything is okay. */
  return GOSP_STATUS_OK;
}

/* Securely concatenate two or more filepaths.  The final entry must be NULL.
 * Return the combined path name or NULL on error. */
char *concatenate_filepaths(server_rec *s, apr_pool_t *pool, ...)
{
  va_list ap;           /* Argument pointer */
  char *merged_name;    /* Path name consisting of pathA followed by pathB */
  char *next_path;      /* Next filepath component */
  apr_status_t status;  /* Status of an APR call */

  /* Process the first argument. */
  va_start(ap, pool);
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
                                pool);
    if (status != APR_SUCCESS) {
      ap_log_error(APLOG_MARK, APLOG_ALERT, status, s,
                   "Failed to securely merge %s and %s", merged_name, next_path);
      return NULL;
    }
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
