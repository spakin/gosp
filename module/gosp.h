/******************************************
 * Header file for the Gosp Apache module *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#ifndef _GOSP_H
#define _GOSP_H

/* Include all other header file here. */
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"


/* Define a macro that checks an error code and, on error, logs a
 * message and returns 0. */
#define REPORT_ERROR(LEVEL, STATUS, ...)				\
do {									\
  if (STATUS == APR_SUCCESS)						\
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|LEVEL, STATUS, r->server, __VA_ARGS__); \
  else									\
    ap_log_error(APLOG_MARK, LEVEL, STATUS, r->server, __VA_ARGS__);	\
  return 0;								\
} while (0)

#endif
