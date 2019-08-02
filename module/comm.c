/******************************************
 * Communicate with a Gosp server process *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#include "gosp.h"

/* Send a string to the socket.  Log a message and return GOSP_STATUS_FAIL on
 * error. */
#define SEND_STRING(...)                                                \
do {                                                                    \
  const char *str = apr_psprintf(r->pool, __VA_ARGS__);                 \
  apr_size_t exp_len = (apr_size_t) strlen(str);                        \
  apr_size_t len = exp_len;                                             \
                                                                        \
  status = apr_socket_send(sock, str, &len);                            \
  if (status != APR_SUCCESS || len != exp_len)                          \
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,                 \
                 "Failed to send %lu bytes to the Gosp server", (unsigned long)exp_len); \
 }                                                                      \
while (0)

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

/* Send HTTP connection information to a socket.  The connection information
 * must be kept up-to-date with the GospRequest struct in boilerplate.go. */
gosp_status_t send_request(apr_socket_t *sock, request_rec *r)
{
  apr_status_t status;    /* Status of an APR call */

  SEND_STRING("{\n");
  SEND_STRING("  \"LocalHostname\": \"%s\"\n", r->hostname);
  SEND_STRING("  \"QueryArgs\": \"%s\"\n", r->args);
  SEND_STRING("  \"PathInfo\": \"%s\"\n", r->path_info);
  SEND_STRING("  \"Uri\": \"%s\"\n", r->uri);
  SEND_STRING("  \"RemoteHostname\": \"%s\"\n",
              ap_get_remote_host(r->connection, r->per_dir_config, REMOTE_NAME, NULL));
  SEND_STRING("}\n");
  return GOSP_STATUS_OK;
}

/* Ask a Gosp server to shut down cleanly.  The termination command must be
 * kept up-to-date with the GospRequest struct in boilerplate.go. */
gosp_status_t send_termination_request(apr_socket_t *sock, request_rec *r)
{
  apr_status_t status;    /* Status of an APR call */

  SEND_STRING("{\n");
  SEND_STRING("  \"ExitNow\": \"true\"\n");
  SEND_STRING("}\n");
  return GOSP_STATUS_OK;
}

/* Split a response string into metadata and data.  Process the metadata.
 * Output the data.  Return GOSP_STATUS_OK if this procedure succeeded (even if
 * it corresponds to a Gosp-server error condition) or GOSP_STATUS_FAIL if
 * not. */
static gosp_status_t process_response_helper(request_rec *r, char *response, size_t resp_len)
{
  char *last;      /* Internal apr_strtok() state */
  char *line;      /* One line of metadata */
  int nwritten;    /* Number of bytes of data written */

  /* Process each line of metadata until we see "end-header". */
  for (line = apr_strtok(response, "\n", &last);
       line != NULL;
       line = apr_strtok(NULL, "\n", &last)) {
    /* End of metadata: exit loop. */
    if (strcmp(line, "end-header") == 0)
      break;

    /* Heartbeat: ignore. */
    if (strcmp(line, "keep-alive") == 0)
      continue;

    /* HTTP status: set in the request_rec. */
    if (strncmp(line, "http-status ", 12) == 0) {
      r->status = atoi(line + 12);
      if (r->status < 100)
        return GOSP_STATUS_FAIL;
      continue;
    }

    /* MIME type: set in the request_rec. */
    if (strncmp(line, "mime-type ", 10) == 0) {
      r->content_type = line + 10;
      continue;
    }

    /* Anything else: throw an error. */
    return GOSP_STATUS_FAIL;
  }

  /* Write the rest of the response as data. */
  if (r->status != HTTP_OK)
    return GOSP_STATUS_OK;
  if (line == NULL)
    return GOSP_STATUS_OK;
  nwritten = ap_rwrite(line, (int)resp_len - (int)(last - response + 1), r);
  if (nwritten != (int)resp_len - (int)(last - response + 1))
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Receive a response from the Gosp server and process it.  Return
 * GOSP_STATUS_NEED_ACTION if the server timed out and ought to be killed and
 * relaunched. */
gosp_status_t process_response(apr_socket_t *sock, request_rec *r)
{
  apr_status_t status;    /* Status of an APR call */
  char *chunk;            /* One chunk of data read from the socket */
  const size_t chunk_size = 1000000;   /* Amount of data to read at once */
  struct iovec iov[2];    /* Pairs of chunks to merge */

  /* Prepare to read from the socket. */
  status = apr_socket_timeout_set(sock, GOSP_RESPONSE_TIMEOUT);
  if (status != APR_SUCCESS)
    REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                 "Failed to set a socket timeout");
  chunk = apr_palloc(r->pool, chunk_size);
  iov[0].iov_base = "";
  iov[0].iov_len = 0;
  iov[1].iov_base = chunk;

  /* Read until the socket is closed. */
  while (status != APR_EOF) {
    apr_size_t len = chunk_size;  /* Number of bytes to read/just read */

    /* Read one chunk of data. */
    status = apr_socket_recv(sock, chunk, &len);
    switch (status) {
    case APR_SUCCESS:
    case APR_EOF:
      /* Successful read */
      break;

    case APR_TIMEUP:
      /* Timeout occurred */
      return GOSP_STATUS_NEED_ACTION;
      break;

    default:
      /* Other error */
      REPORT_ERROR(GOSP_STATUS_FAIL, APLOG_ALERT, status,
                   "Failed to receive data from the Gosp server");
      break;
    }

    /* Append the new chunk onto the aggregate response. */
    iov[1].iov_len = (size_t) len;
    iov[0].iov_base = apr_pstrcatv(r->pool, iov, 2, &len);
    iov[0].iov_len = (size_t) len;
  }
  return process_response_helper(r, iov[0].iov_base, iov[0].iov_len);
}
