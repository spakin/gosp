/******************************************
 * Communicate with a Gosp server process *
 *                                        *
 * By Scott Pakin <scott+gosp@pakin.org>  *
 ******************************************/

#include "gosp.h"

/* Send a string to the socket.  Log a message and return GOSP_STATUS_FAIL on
 * error. */
#define SEND_STRING(...)                                                \
  do {                                                                  \
    const char *str = apr_psprintf(r->pool, __VA_ARGS__);               \
    apr_size_t exp_len = (apr_size_t) strlen(str);                      \
    apr_size_t len = exp_len;                                           \
                                                                        \
    status = apr_socket_send(sock, str, &len);                          \
    if (status != APR_SUCCESS || len != exp_len)                        \
      REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,         \
                          "Failed to send %lu bytes to the Gosp server", (unsigned long)exp_len); \
  }                                                                     \
  while (0)

/* Connect to a Unix-domain stream socket.  Return GOSP_STATUS_FAIL if we fail
 * to create any local data structures.  Return GOSP_STATUS_NEED_ACTION if we
 * fail to connect to the socket.  Return GOSP_STATUS_OK on success. */
gosp_status_t connect_socket(request_rec *r, const char *sock_name, apr_socket_t **sock)
{
  apr_sockaddr_t *sa;         /* Socket address corresponding to sock_name */
  apr_status_t status;        /* Status of an APR call */

  /* Construct a socket address. */
  status = apr_sockaddr_info_get(&sa, sock_name, APR_UNIX, 0, 0, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                         "Failed to construct a Unix-domain socket address from %s", sock_name);

  /* Create a Unix-domain stream socket. */
  status = apr_socket_create(sock, APR_UNIX, SOCK_STREAM, APR_PROTO_TCP, r->pool);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                         "Failed to create socket %s", sock_name);

  /* Connect to the socket we just created.  Failure presumably indicates that
   * the Gosp server isn't running. */
  status = apr_socket_connect(*sock, sa);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_NEED_ACTION, APLOG_INFO, status,
                         "Failed to connect to socket %s", sock_name);
  return GOSP_STATUS_OK;
}

/* Escape a string for JSON. */
static char *escape_for_json(request_rec *r, const char *str)
{
  char *quoted;    /* Quoted version of str */
  const char *sp;  /* Pointer into str */
  char *qp;        /* Pointer into quoted */

  if (str == NULL)
    return apr_pstrdup(r->pool, "");
  quoted = apr_palloc(r->pool, strlen(str)*2 + 1);  /* Worst-case allocation */
  for (qp = quoted, sp = str; *sp != '\0'; qp++, sp++)
    switch (*sp) {
    case '\\':
    case '"':
      *qp++ = '\\';
      *qp = *sp;
      break;

    default:
      *qp = *sp;
      break;
    }
  *qp = '\0';
  return quoted;
}

/* Send POST data as a JSON object. */
static gosp_status_t send_post_data(request_rec *r, apr_socket_t *sock)
{
  apr_array_header_t *array = NULL;   /* Array of key:value pairs */
  int first = 1;              /* 1=first key:value pair; 0=subsequent pair */
  apr_status_t status;        /* Status of an APR call */

  /* Read the POST data into an array. */
  status = ap_parse_form_data(r, NULL, &array, -1, GOSP_MAX_POST_SIZE);
  if (status != APR_SUCCESS)
    return GOSP_STATUS_FAIL;
  if (array == NULL)
    return GOSP_STATUS_OK;    /* No POST data */

  /* Walk the array key:value entry by key:value entry. */
  SEND_STRING("  \"PostData\": {");
  while (array && !apr_is_empty_array(array)) {
    apr_off_t blen;    /* Length of the brigade used for the value */
    apr_size_t slen;   /* Length of the value itself */
    char *value;       /* The value converted from a brigade to a string */

    /* Get the next pair, and convert its value from a brigade to a string. */
    ap_form_pair_t *pair = (ap_form_pair_t *) apr_array_pop(array);
    apr_brigade_length(pair->value, 1, &blen);
    slen = (apr_size_t) blen;
    value = apr_palloc(r->pool, slen + 1);
    apr_brigade_flatten(pair->value, value, &slen);
    value[slen] = 0;

    /* Output the key and value as JSON code. */
    if (first)
      first = 0;
    else
      SEND_STRING("\n,");
    SEND_STRING("    \"%s\": \"%s\"",
                escape_for_json(r, pair->name), escape_for_json(r, value));
  }
  SEND_STRING("\n  },\n");
  return GOSP_STATUS_OK;
}

/* Define some information to pass to a table iterator. */
typedef struct {
  request_rec *request;   /* Current request */
  int first;              /* 1=first item in table; 0=subsequent item */
  apr_socket_t *socket;   /* Gosp socket to send strings to */
} table_item_data_t;

/* Send a {key, value} pair with JSON encoding into a socket. */
static int send_table_item(void *rec, const char *key, const char *value)
{
  const char *str;            /* String to send */
  apr_size_t exp_len;         /* Number of bytes we had expected to send */
  apr_size_t len;             /* Number of bytes sent */
  apr_status_t status;        /* Status of an APR call */

  /* Extract a few fields from our data structure. */
  table_item_data_t *data = (table_item_data_t *)rec;
  request_rec *r = data->request;
  apr_socket_t *sock = data->socket;

  /* Send the key and value as JSON code. */
  if (data->first)
    data->first = 0;
  else {
    len = 2;
    status = apr_socket_send(sock, ",\n", &len);
    if (status != APR_SUCCESS || len != 2)
      REPORT_REQUEST_ERROR(0, APLOG_ERR, status,
                           "Failed to send 2 bytes to the Gosp server");
  }
  str = apr_psprintf(r->pool, "      \"%s\": \"%s\"",
                     escape_for_json(r, key), escape_for_json(r, value));
  len = exp_len = (apr_size_t) strlen(str);
  status = apr_socket_send(sock, str, &len);
  if (status != APR_SUCCESS || len != exp_len)
    REPORT_REQUEST_ERROR(0, APLOG_ERR, status,
                         "Failed to send %ld bytes to the Gosp server", exp_len);
  return 1;
}

/* Send an entire table into a socket as JSON-encoded {key, value} pairs. */
static int send_table(request_rec *r, apr_socket_t *sock,
                      const char *field_name, apr_table_t *table)
{
  table_item_data_t item_data;  /* Data to pass to each table item */
  apr_status_t status;          /* Status of an APR call */

  /* Initialize the table walk. */
  item_data.request = r;
  item_data.socket = sock;
  item_data.first = 1;

  /* Walk the table, sending each {key, value} pair in turn. */
  SEND_STRING("    \"%s\": {", field_name);
  if (apr_table_do(send_table_item, (void *) &item_data, table, NULL) == FALSE)
    return GOSP_STATUS_FAIL;
  SEND_STRING("\n    },\n");
  return GOSP_STATUS_OK;
}

/* Parse the GET arguments into a table.  Return NULL on failure. */
static apr_table_t *parse_get_args(request_rec *r)
{
  apr_table_t *tbl;   /* Table of {key, value} mappings */
  char *query;        /* GET query */
  char *kvstr;        /* A single "key=value" string */
  char *amplast;      /* apr_strtok() state associated with kvstr */
  char *key;          /* A single key */
  char *value;        /* A single value */
  char *eqlast;       /* apr_strtok() state associated with key */

  /* Prepare our input string and output table. */
  query = apr_pstrdup(r->pool, r->args);
  tbl = apr_table_make(r->pool, 10);  /* 10 is arbitrary. */

  /* Parse the query arguments. */
  if (r->args == NULL)
    return tbl;
  while (1) {
    /* Split the query into zero or more "&"-separated strings. */
    kvstr = apr_strtok(query, "&", &amplast);
    if (kvstr == NULL)
      break;
    query = NULL;

    /* Parse the query as "key=value".  Either key or value or both can be
     * empty. */
    if (kvstr[0] == '=') {
      /* Weird case, but we ought to check for it. */
      key = "";
      value = apr_strtok(kvstr, "=", &eqlast);
    }
    else {
      key = apr_strtok(kvstr, "=", &eqlast);
      if (key == NULL)
        continue;
      value = eqlast;
    }
    if (value == NULL)
      value = "";

    /* Store the {key, value} pair in the table. */
    apr_table_set(tbl, key, value);
  }
  return tbl;
}

/* Send HTTP connection information to a socket.  The connection information
 * must be kept up-to-date with the GospRequest struct in boilerplate.go. */
gosp_status_t send_request(request_rec *r, apr_socket_t *sock)
{
  apr_status_t status;          /* Status of an APR call */
  const char *rhost;            /* Name of remote host */
  const char *lhost;            /* Name of local host as used in the request */
  int port;                     /* Port number to which the request was issued */
  const char *url;              /* Complete URL requested */
  apr_table_t *get_data;        /* Parsed GET query */

  /* Prepare some data we'll need below. */
  rhost = ap_get_remote_host(r->connection, r->per_dir_config, REMOTE_NAME, NULL);
  lhost = ap_get_server_name_for_url(r);
  port = ap_get_server_port(r);
  get_data = parse_get_args(r);

  /* For the Gosp page's convenience, combine the various URL components into a
   * complete URL. */
  url = apr_psprintf(r->pool, "%s://%s", ap_http_scheme(r), lhost);
  if (!ap_is_default_port(port, r))
    url = apr_psprintf(r->pool, "%s:%d", url, port);
  url = apr_psprintf(r->pool, "%s%s", url, r->uri);
  if (r->args != NULL && r->args[0] != '\0')
    url = apr_psprintf(r->pool, "%s?%s", url, r->args);

  /* Send the request as JSON-encoded data. */
  SEND_STRING("{\n");
  SEND_STRING("  \"UserData\": {\n");
  SEND_STRING("    \"Scheme\": \"%s\",\n", escape_for_json(r, ap_http_scheme(r)));
  SEND_STRING("    \"LocalHostname\": \"%s\",\n", escape_for_json(r, lhost));
  SEND_STRING("    \"Port\": %d,\n", port);
  SEND_STRING("    \"Uri\": \"%s\",\n", escape_for_json(r, r->uri));
  SEND_STRING("    \"PathInfo\": \"%s\",\n", escape_for_json(r, r->path_info));
  SEND_STRING("    \"QueryArgs\": \"%s\",\n", escape_for_json(r, r->args));
  SEND_STRING("    \"Url\": \"%s\",\n", escape_for_json(r, url));
  SEND_STRING("    \"Method\": \"%s\",\n", escape_for_json(r, r->method));
  SEND_STRING("    \"RequestLine\": \"%s\",\n", escape_for_json(r, r->the_request));
  SEND_STRING("    \"RequestTime\": %" PRId64 ",\n", r->request_time*1000);
  SEND_STRING("    \"RemoteHostname\": \"%s\",\n", escape_for_json(r, rhost));
  SEND_STRING("    \"RemoteIp\": \"%s\",\n", escape_for_json(r, r->useragent_ip));
  SEND_STRING("    \"Filename\": \"%s\",\n", escape_for_json(r, r->filename));
  if (send_post_data(r, sock) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  if (send_table(r, sock, "GetData", get_data) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  if (send_table(r, sock, "HeaderData", r->headers_in) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  if (send_table(r, sock, "Environment", r->subprocess_env) != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  SEND_STRING("    \"AdminEmail\": \"%s\"\n", escape_for_json(r, r->server->server_admin));
  SEND_STRING("  }\n");
  SEND_STRING("}\n");
  return GOSP_STATUS_OK;
}

/* Ask a Gosp server to shut down cleanly. */
gosp_status_t send_termination_request(request_rec *r, const char *sock_name)
{
  char *response;             /* Response string */
  size_t resp_len;            /* Length of response string */
  apr_proc_t proc;            /* Gosp server process */
  apr_time_t begin_time;      /* Time at which we began waiting for the server to terminate */
  apr_socket_t *sock;         /* Socket with which to communicate with the Gosp server */
  gosp_status_t gstatus;      /* Status of an internal Gosp call */
  apr_status_t status;        /* Status of an APR call */

  /* Connect to the process that handles the requested Go Server Page. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, APR_SUCCESS, r,
               "Asking the Gosp server listening on socket %s to terminate",
               sock_name);
  gstatus = connect_socket(r, sock_name, &sock);
  if (gstatus != GOSP_STATUS_OK)
    return GOSP_STATUS_NEED_ACTION;

  /* Ask the server to terminate. */
  SEND_STRING("{\n");
  SEND_STRING("  \"ExitNow\": true\n");
  SEND_STRING("}\n");

  /* Receive a process ID in response. */
  gstatus = receive_response(r, sock, &response, &resp_len);
  if (gstatus != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  if (strncmp(response, "gosp-pid ", 9) != 0)
    return GOSP_STATUS_FAIL;
  proc.pid = atoi(response + 9);
  if (proc.pid <= 0)
    return GOSP_STATUS_FAIL;

  /* We no longer need the socket. */
  status = apr_socket_close(sock);
  if (status != APR_SUCCESS)
    return GOSP_STATUS_FAIL;

  /* Wait for a short time for the process to exit by itself. */
  begin_time = apr_time_now();
  while (apr_time_now() - begin_time < GOSP_EXIT_WAIT_TIME) {
    /* Ping the process.  If it's not found, it must have exited on its own. */
    status = apr_proc_kill(&proc, 0);
    if (APR_TO_OS_ERROR(status) == ESRCH)
      return GOSP_STATUS_OK;
    apr_sleep(1000);
  }

  /* The process did not exist by itself.  Kill it. */
  status = apr_proc_kill(&proc, SIGKILL);
  if (status != APR_SUCCESS)
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Parse and process an HTTP header field assignment. */
static gosp_status_t process_field_assignment(request_rec *r, char *line)
{
  char *orig_line;  /* Copy of line for use in error messages */
  char *replace;    /* Replace existing key if "true"; append if "false" */
  char *key;        /* Name of header field */
  char *value;      /* Value to assign to header field */
  char *last;       /* apr_strtok() state */

  /* Parse a line of the form "header-field true MyKey MyValue" into
   * {replace, key, value}. */
  orig_line = apr_pstrdup(r->pool, line);
  replace = apr_strtok(line + 13, " ", &last);
  if (replace == NULL)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "Failed to parse \"%s\"", orig_line);
  key = apr_strtok(NULL, " ", &last);
  if (key == NULL)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "Failed to parse \"%s\"", orig_line);
  value = last;

  /* Either append or replace the key in the headers_out table. */
  if (strcmp(replace, "true") == 0)
    apr_table_set(r->headers_out, key, value);
  else
    if (strcmp(replace, "false") == 0)
      apr_table_add(r->headers_out, key, value);
    else
      REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                           "Expected \"true\" or \"false\" but saw \"%s\" in \"%s\"",
                           replace, orig_line);
  return GOSP_STATUS_OK;
}

/* Split a response string into metadata and data.  Process the metadata.
 * Output the data.  Return GOSP_STATUS_OK if this procedure succeeded (even if
 * it corresponds to a Gosp-server error condition) or GOSP_STATUS_FAIL if
 * not. */
static gosp_status_t process_response(request_rec *r, char *response, size_t resp_len)
{
  char *last;      /* Internal apr_strtok() state */
  char *line;      /* One line of metadata */
  int n_to_write;  /* Number of bytes of data we expect to write */
  int nwritten;    /* Number of bytes of data actuall written */

  /* Process each line of metadata until we see "end-header". */
  for (line = apr_strtok(response, "\n", &last);
       line != NULL;
       line = apr_strtok(NULL, "\n", &last)) {
    /* End of metadata: exit loop. */
    if (strcmp(line, "end-header") == 0)
      break;

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

    /* Header field: set in the request_rec. */
    if (strncmp(line, "header-field ", 13) == 0) {
      if (process_field_assignment(r, line) != GOSP_STATUS_OK)
        return GOSP_STATUS_FAIL;
      continue;
    }

    /* Error message: output it. */
    if (strncmp(line, "error-message ", 14) == 0) {
      ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, APR_SUCCESS, r,
                    "%s", line + 14);
      continue;
    }

    /* Debug message: output it. */
    if (strncmp(line, "debug-message ", 14) == 0) {
      ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, APR_SUCCESS, r,
                    "%s", line + 14);
      continue;
    }

    /* Heartbeat: ignore. */
    if (strcmp(line, "keep-alive") == 0)
      continue;

    /* Anything else: throw an error. */
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, APR_SUCCESS,
                         "Received unexpected metadata command \"%s\"", line);
  }

  /* Write the rest of the response as data. */
  if (r->status != HTTP_OK)
    return GOSP_STATUS_OK;
  if (line == NULL)
    return GOSP_STATUS_OK;
  n_to_write = (int)resp_len - (int)(last - response + 1) + 1;
  nwritten = ap_rwrite(line + 11, n_to_write, r);
  if (nwritten != n_to_write)
    return GOSP_STATUS_FAIL;
  return GOSP_STATUS_OK;
}

/* Receive a response from the Gosp server and return it.  Return
 * GOSP_STATUS_NEED_ACTION if the server timed out and ought to be killed and
 * relaunched. */
gosp_status_t receive_response(request_rec *r, apr_socket_t *sock, char **response, size_t *resp_len)
{
  char *chunk;                /* One chunk of data read from the socket */
  const size_t chunk_size = 1000000;   /* Amount of data to read at once */
  struct iovec iov[2];        /* Pairs of chunks to merge */
  apr_status_t status;        /* Status of an APR call */

  /* Prepare to read from the socket. */
  status = apr_socket_timeout_set(sock, GOSP_RESPONSE_TIMEOUT);
  if (status != APR_SUCCESS)
    REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
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
    case APR_EOF:
    case APR_SUCCESS:
      /* Successful read */
      break;

    case APR_TIMEUP:
      /* Timeout occurred */
      return GOSP_STATUS_NEED_ACTION;
      break;

    default:
      /* Other error */
      REPORT_REQUEST_ERROR(GOSP_STATUS_FAIL, APLOG_ERR, status,
                           "Failed to receive data from the Gosp server");
      break;
    }

    /* Append the new chunk onto the aggregate response. */
    iov[1].iov_len = (size_t) len;
    iov[0].iov_base = apr_pstrcatv(r->pool, iov, 2, &len);
    iov[0].iov_len = (size_t) len;
  }

  /* Return the string and its length. */
  *response = iov[0].iov_base;
  *resp_len = iov[0].iov_len;
  return GOSP_STATUS_OK;
}

/* Send a request to the Gosp server and process its response.  If the server
 * is not currently running, return GOSP_STATUS_NEED_ACTION.  This function is
 * intended to represent the common case in processing HTTP requests to Gosp
 * pages. */
gosp_status_t simple_request_response(request_rec *r, const char *sock_name)
{
  apr_socket_t *sock;         /* The Unix-domain socket proper */
  char *response;             /* Response string */
  size_t resp_len;            /* Length of response string */
  apr_status_t status;        /* Status of an APR call */
  gosp_status_t gstatus;      /* Status of an internal Gosp call */

  /* Connect to the process that handles the requested Go Server Page. */
  ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, APR_SUCCESS, r,
               "Asking the Gosp server listening on socket %s to handle URI %s",
               sock_name, r->uri);
  gstatus = connect_socket(r, sock_name, &sock);
  if (gstatus != GOSP_STATUS_OK)
    return gstatus;

  /* Send the Gosp server a request and process its response. */
  gstatus = send_request(r, sock);
  if (gstatus != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  gstatus = receive_response(r, sock, &response, &resp_len);
  if (gstatus != GOSP_STATUS_OK)
    return GOSP_STATUS_FAIL;
  status = apr_socket_close(sock);
  if (status != APR_SUCCESS)
    return GOSP_STATUS_FAIL;
  return process_response(r, response, resp_len);
}
