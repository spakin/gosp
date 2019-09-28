/*
Package gosp provides all of the types, functions, and values needed to compile
a generated Gosp server.  RequestData, Open, and perhaps LiesInOrBelow are the
only things defined by this package that an end user should care about.
Everything else is used internally by the Gosp server.
*/
package gosp

import (
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime/debug"
	"strings"
)

// The following data structure must be kept up-to-date with the
// send_request() function in mod_gosp's comm.c.

// RequestData encapsulates Web-server information passed into a Gosp server.
// Many of its fields come from the client.  Those should therefore not be used
// without first checking them for invalid or malicious content.
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

// KeyValue represents a metadata key:value pair.
type KeyValue struct {
	Key   string
	Value string
}

// SetHttpStatus tells the Web server what HTTP status code it should return.
func SetHttpStatus(ch chan<- KeyValue, s int) {
	ch <- KeyValue{Key: "http-status", Value: fmt.Sprint(s)}
}

// SetMimeType tells the Web server what MIME type it should return.
func SetMimeType(ch chan<- KeyValue, mt string) {
	ch <- KeyValue{Key: "mime-type", Value: mt}
}

// SetHeaderField provides a field name and value for the Web server to include
// in the HTTP header.  If repl is true, the value replaces any prior value for
// the field.  If repl is false, the field and value are appended to the
// existing set of {field name, value} pairs.
func SetHeaderField(ch chan<- KeyValue, k, v string, repl bool) {
	ch <- KeyValue{
		Key:   "header-field",
		Value: fmt.Sprintf("%v %s %s", repl, k, v),
	}
}

// ReportPanic alerts the Web server that the Gosp server encountered an
// unexpected error.  It should be called from a deferred function in
// GospGenerateHTML.
func ReportPanic(r interface{}, ch chan<- KeyValue) {
	ch <- KeyValue{Key: "error-message", Value: fmt.Sprint(r)}
	ch <- KeyValue{Key: "error-message", Value: "+------------------------------------------------------------"}
	for _, tr := range strings.Split(string(debug.Stack()), "\n") {
		if tr == "" {
			continue
		}
		tr = strings.Replace(tr, "\t", "        ", 1) // Apache doesn't honor tab characters.
		ch <- KeyValue{Key: "error-message", Value: "| " + tr}
	}
	ch <- KeyValue{Key: "error-message", Value: "+------------------------------------------------------------"}
	SetHttpStatus(ch, http.StatusInternalServerError)
}

// Writer is merely a renamed io.Writer.
type Writer interface {
	io.Writer
}

// Fprintf is merely a renamed fmt.Fprintf.
func Fprintf(w Writer, format string, a ...interface{}) (n int, err error) {
	return fmt.Fprintf(w, format, a...)
}

// evalPartialSymlinks is like filepath.EvalSymlinks but can handle
// nonexistent files.
func evalPartialSymlinks(fn string) (string, error) {
	if fn == "" {
		return "", nil
	}
	real, err := filepath.EvalSymlinks(fn)
	if errors.Is(err, os.ErrNotExist) {
		// The file doesn't exist.  Expand symlinks in its parent.
		dir, file := filepath.Split(fn)
		if dir == fn {
			return dir, nil // Top-level directory was not found.
		}
		realDir, err := evalPartialSymlinks(dir)
		if err != nil {
			return "", err
		}
		return filepath.Join(realDir, file), nil
	}
	return real, err
}

// realPath returns a canonicalized absolute pathname containing no symbolic
// links.
func realPath(fn string) (string, error) {
	noSym, err := evalPartialSymlinks(fn)
	if err != nil {
		return "", err
	}
	abs, err := filepath.Abs(noSym)
	if err != nil {
		return "", err
	}
	return abs, nil
}

// LiesInOrBelow returns true if a child file or directory lies in or below a
// parent directory (or is the parent directory).
func LiesInOrBelow(child, parent string) (bool, error) {
	// Expand the parent and directory to absolute, non-symlinked paths.
	p, err := realPath(parent)
	if err != nil {
		return false, err
	}
	c, err := realPath(child)
	if err != nil {
		return false, err
	}

	// Return true if the child begins with the parent path, false
	// otherwise.
	switch {
	case len(c) < len(p):
		return false, nil
	case c == p:
		return true, nil
	case c[:len(p)] == p && c[len(p)] == filepath.Separator:
		return true, nil
	case p == string(filepath.Separator):
		return true, nil
	default:
		return false, nil
	}
}

// Open is a wrapper for os.Open that allows opening only those files that lie
// within or below the current directory.
func Open(name string) (*os.File, error) {
	cwd, err := os.Getwd()
	if err != nil {
		return nil, err
	}
	in, err := LiesInOrBelow(name, cwd)
	if err != nil {
		return nil, err
	}
	if !in {
		return nil, os.ErrPermission
	}
	return os.Open(name)
}
