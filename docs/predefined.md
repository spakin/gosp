---
title: Predefined items
nav_order: 5
---

Predefined packages and variables
=================================

A Go Server Page has available to it one package and a few variables that do not need to be declared explicitly.  These are described below.

All predefined and internal values have names beginning with either `Gosp` or `gosp`.  To avoid introducing name conflicts it is recommended that you not declare your own values with either of those prefixes.

Packages
--------

The `gosp` package is automatically imported.  It defines a few functions intended for internal use by the Go code generated from a Go Server Page and a few values that can be of use to the user-written Go code appearing on a Go Server Page.

The most useful declaration is `gosp.RequestData`, a type that encapsulates Web-server information passed to a Go Server Page.  Note that many of its fields come from the client.  Those should therefore not be used without first checking them for invalid or malicious content.
```go
type RequestData struct {
	Scheme         string            // HTTP scheme ("http" or "https")
	LocalHostname  string            // Name of the local host
	Port           int               // Port number to which the request was issued
	Uri            string            // Path portion of the URI
	PathInfo       string            // Additional text following the Gosp filename
	QueryArgs      string            // Query arguments from the request
	Url            string            // Complete URL requested
	Method         string            // Request method ("GET", "POST", etc.)
	RequestLine    string            // First line of the request (e.g., "GET / HTTP/1.1")
	RequestTime    int64             // Request time in nanoseconds since the Unix epoch
	RemoteHostname string            // Name of the remote host
	RemoteIp       string            // IP address of the remote host
	Filename       string            // Local filename of the Gosp page
	PostData       map[string]string // {Key, value} pairs sent by a POST request
	GetData        map[string]string // {Key, value} pairs sent by a GET request (parsed version of QueryArgs)
	HeaderData     map[string]string // Request headers as {key, value} pairs
	AdminEmail     string            // Email address of the Web server administrator
	Environment    map[string]string // Environment variables passed in from the server
}
```

The `gosp` package makes available three functions that can be used to pass metadata back to the Web server:
```go
func SetHttpStatus(m Metadata, s int)
func SetMimeType(m Metadata, mt string)
func SetHeaderField(m Metadata, k, v string, repl bool)
```
Always pass the variable `gospMeta` (see Variables below) as the first argument to each of these three functions.

`gosp.SetHttpStatus` indicates the HTTP status code the Web server should return to the client.  The status code defaults to 200 ("OK") but can be set to any of the codes defined in the [HTTP Status Code Registry](https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml).  If the Go Server Page is allowed to `import "net/http"` it can use any of the [`Status` constants defined by `net/http`](https://golang.org/pkg/net/http/#pkg-constants) instead of specifying a status code numerically.  Calling `gosp.SetHttpStatus` repeatedly is allowed.  Only the final value takes effect.

`gosp.SetMimeType` specifies the document's [MIME type](https://en.wikipedia.org/wiki/Media_type).  The MIME type defaults to `text/html`.  A Go Server Page can thereby change the MIME type and return data of that type.  For example, it can specify `text/plain` and return plain text or `image/png` and return a binary [PNG image](https://en.wikipedia.org/wiki/Portable_Network_Graphics).  Calling `gosp.SetMimeType` repeatedly is allowed.  Only the final value takes effect.

`gosp.SetHeaderField` provides a very general capability.  It enables a Go Server Page to send arbitrary [HTTP response header fields](https://en.wikipedia.org/wiki/List_of_HTTP_header_fields#Standard_response_fields) back to the client.  The arguments are the field name, the value to assign to that field, and a Boolean that indicates whether the value should replace the prior value associated with the field (`true`) as opposed to being appended to the prior value (`false`).  Some uses of `gosp.SetHeaderField` include transmitting cookies to the client and setting caching properties for the page.  Calling `gosp.SetHeaderField` repeatedly is allowed.

Other useful exports from the `gosp` package include `gosp.Fprintf`, `gosp.Writer`, and `gosp.Open`.  `gosp.Fprintf` is exactly the same as [`fmt.Fprintf`](https://golang.org/pkg/fmt/#Fprintf) but does not require importing the [`fmt`](https://golang.org/pkg/fmt) package.  (As mentioned in [Configuring Go Server Pages](configure.md), package imports other than `gosp` are forbidden unless explicitly allowed by the Web administrator.)  Similarly, `gosp.Writer` wraps [`io.Writer`](https://golang.org/pkg/io/#Writer) without requiring that a page import the [`io`](https://golang.org/pkg/io) package.  `gosp.Open` behaves similarly to [`os.Open`](https://golang.org/pkg/os/#Open).  However, only files that lie in the same directory or a subdirectory of the Go Server Page that invokes `gosp.Open` can be opened.  A file can be checked explicitly for this property with the `gosp.LiesInOrBelow` function.

Variables
---------

Three variables that are available within all `?go:block` and `?go:expr` markup are `gospReq`, `gospOut`, and `gospMeta`.  `gospReq` is of type `*gosp.RequestData` (presented above) and encapsulates a large set of data provided by the Web server in response to a client request.  Many of the fields correspond to parts of the URL provided by the client and should therefore be sanitized before use in any sensitive operation.  `gospOut` is a `gosp.Writer` that represents the contents of the Go Server Page.  Writing to `gospOut` (e.g., with `gosp.Fprintf(gospOut, …)`) injects text into the page right where the `?go:block` or `?go:expr` appeared.  `gospMeta`, of type `gosp.Metadata`, should be treated as an opaque value that is used only as the first argument to `gosp.SetHttpStatus`, `gosp.SetMimeType`, and `gosp.SetHeaderField`.

Functions defined by `?go:top` markup do not have access to `gospReq`, `gospOut`, or `gospMeta`.  These must be passed in as parameters if needed.
