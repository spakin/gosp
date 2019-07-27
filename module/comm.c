/******************************************
 * Communicate with a Gosp server process *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#include "gosp.h"

/* Connect to a Unix-domain stream socket. */
gosp_status_t connect_socket(apr_socket_t **sock, request_rec *r, const char *sock_name)
{
  apr_status_t status;    /* Status of an APR call */
  apr_sockaddr_t *sa;     /* Socket address corresponding to sock_name */

  /* Construct a socket address. */
  status = apr_sockaddr_info_get(&sa, sock_name, APR_UNIX, 0, 0, r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to construct a Unix-domain socket address from %s", sock_name);

  /* Create a Unix-domain stream socket. */
  status = apr_socket_create(sock, APR_UNIX, SOCK_STREAM, APR_PROTO_TCP, r->pool);
  if (status != APR_SUCCESS)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to create socket %s", sock_name);

  /* Connect to the socket we just created. */
  status = apr_socket_connect(*sock, sa);
  if (status != APR_SUCCESS)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to connect to socket %s", sock_name);
  return GOSP_STATUS_OK;
}
