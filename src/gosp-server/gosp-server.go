// gosp-server launches a server process that accepts requests on a local
// socket and sends back metadata followed by data (e.g., HTML).  As an option,
// it can generate the data directly as a one-shot execution rather than from a
// persistent server process.
package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"gosp"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"plugin"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unicode"
)

// Version defines the Go Server Pages version number.  It should be overridden
// by the Makefile.
var Version = "?.?.?"

// notify is used to output error messages.
var notify *log.Logger

// A ServiceRequest is a request for service sent to us by the Web server.
type ServiceRequest struct {
	UserData gosp.RequestData // Data to pass to the user's code
	ExitNow  bool             // If true, shut down the program cleanly
}

// okStr represents an HTTP success code as a string.
var okStr = fmt.Sprint(http.StatusOK)

// writeRawMetadata is a helper routine for LaunchHTMLGenerator that writes
// HTTP metadata in raw HTTP format.  It returns an HTTP status as a string.
func writeRawMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	// Define default header values.
	contentType := "text/html"
	headers := make([]string, 0, 3)
	status := okStr

	// Read metadata from GospGenerateHTML until no more remains.
	for kv := range meta {
		switch kv.Key {
		case "mime-type":
			contentType = kv.Value
		case "http-status":
			status = kv.Value
		case "header-field":
			headers = append(headers, kv.Value)
		}
	}

	// Output the metadata.
	fmt.Fprintf(gospOut, "Content-type: %s\n", contentType)
	for _, h := range headers {
		fmt.Fprintln(gospOut, h)
	}
	fmt.Fprintln(gospOut, "")
	return status
}

// writeNoMetadata is a helper routine for LaunchHTMLGenerator that discards
// all metadata.  It might be used when postprocessing the output of CGI
// scripts, which already output an HTTP header.  writeNoMetadata returns an
// HTTP status as a string.
func writeNoMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	status := okStr
	for kv := range meta {
		if kv.Key == "http-status" {
			status = kv.Value
		}
	}
	return status
}

// sanitizeString is a helper routine for writeModGospMetadata that replaces a
// string's newlines, tabs, and other whitespace characters with ordinary
// spaces.
func sanitizeString(s string) string {
	return strings.Map(func(r rune) rune {
		if unicode.IsSpace(r) {
			return ' '
		}
		return r
	}, s)
}

// writeModGospMetadata is a helper routine for LaunchHTMLGenerator that writes
// HTTP metadata in the format expected by the Gosp Apache module.  It returns
// an HTTP status as a string.
func writeModGospMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	// Read metadata from GospGenerateHTML until no more remains.
	status := okStr
	for kv := range meta {
		switch kv.Key {
		case "mime-type", "http-status", "header-field", "keep-alive", "error-message", "debug-message":
			k := sanitizeString(kv.Key)
			v := sanitizeString(kv.Value)
			fmt.Fprintln(gospOut, k, v)
		}

		// Keep track of the current HTTP status code.
		if kv.Key == "http-status" {
			status = kv.Value
		}
	}
	fmt.Fprintln(gospOut, "end-header")
	return status
}

// LaunchHTMLGenerator starts GospGenerateHTML in a separate goroutine and
// waits for it to finish.
func LaunchHTMLGenerator(p *Parameters, gospOut io.Writer, gospReq *gosp.RequestData) {
	// Spawn GospGenerateHTML, giving it a buffer in which to write HTML
	// and a channel in which to send metadata.
	html := bytes.NewBuffer(nil)
	meta := make(chan gosp.KeyValue, 5)
	go p.GospGenerateHTML(gospReq, html, meta)

	// Tell the server what we think its JSON request is.
	meta <- gosp.KeyValue{
		Key:   "debug-message",
		Value: sanitizeString(fmt.Sprintf("Handling %#v", gospReq)),
	}

	// Read metadata from GospGenerateHTML until no more remains.
	var status string
	switch p.HTTPHeaderType {
	case "mod_gosp":
		status = writeModGospMetadata(gospOut, meta)
	case "raw":
		status = writeRawMetadata(gospOut, meta)
	case "none":
		status = writeNoMetadata(gospOut, meta)
	}

	// Write the generated HTML, but only on success.
	if status == okStr {
		fmt.Fprint(gospOut, html)
	}
}

// chdirOrAbort changes to the directory containing the Go Server Page.  It
// aborts on error.
func chdirOrAbort(fn string) {
	if fn == "" {
		return
	}
	err := os.Chdir(filepath.Dir(fn))
	if err != nil {
		notify.Fatal(err)
	}
}

// GospRequestFromFile reads a gosp.Request from a named JSON file and passes
// this to LaunchHTMLGenerator.
func GospRequestFromFile(p *Parameters) error {
	f, err := os.Open(p.FileName)
	if err != nil {
		return err
	}
	defer f.Close()
	dec := json.NewDecoder(f)
	var sr ServiceRequest
	err = dec.Decode(&sr)
	if err != nil {
		return err
	}
	chdirOrAbort(sr.UserData.Filename)
	LaunchHTMLGenerator(p, os.Stdout, &sr.UserData)
	return nil
}

// ResetKillClock adds time to the auto-kill clock.
func ResetKillClock(t *time.Timer, d time.Duration) {
	if d == 0 {
		return
	}
	if !t.Stop() {
		<-t.C
	}
	t.Reset(d)
}

// StartServer runs the program in server mode.  It accepts a connection on
// a Unix-domain socket, reads a gosp.Request in JSON format, and spawns
// LaunchHTMLGenerator to respond to the request.  The server terminates
// when given a request with ExitNow set to true.
func StartServer(p *Parameters) error {
	// Server code should write only to the io.Writer it's given and not
	// read at all.
	_ = os.Stdin.Close()
	_ = os.Stdout.Close()

	// Listen on the named Unix-domain socket.
	sock, err := filepath.Abs(p.SocketName)
	if err != nil {
		return err
	}
	_ = os.Remove(sock) // It's not an error if the socket doesn't exist.
	ln, err := net.Listen("unix", sock)
	if err != nil {
		return err
	}

	// Exit automatically after AutoKillTime time of no activity.
	var killClk *time.Timer
	if p.AutoKillTime > 0 {
		killClk = time.AfterFunc(p.AutoKillTime, func() {
			_ = os.Remove(sock)
			os.Exit(0)
		})
	}

	// Process connections until we're told to stop.
	var done int32
	var wg sync.WaitGroup
	for atomic.LoadInt32(&done) == 0 {
		// Spawn a goroutine to handle the incoming connection.
		conn, err := ln.Accept()
		if err != nil {
			return err
		}
		ResetKillClock(killClk, p.AutoKillTime)
		wg.Add(1)
		go func(conn net.Conn) {
			// Parse the request as a JSON object.
			defer wg.Done()
			defer conn.Close()
			err = conn.SetDeadline(time.Now().Add(10 * time.Second))
			if err != nil {
				return
			}
			dec := json.NewDecoder(conn)
			var sr ServiceRequest
			err = dec.Decode(&sr)
			if err != nil {
				return
			}

			// If we were asked to exit, send back our PID, notify
			// our parent, and establish a dummy connection to
			// flush the pipeline.
			if sr.ExitNow {
				fmt.Fprintf(conn, "gosp-pid %d\n", os.Getpid())
				atomic.StoreInt32(&done, 1)
				c, err := net.Dial("unix", sock)
				if err == nil {
					_ = c.Close()
				}
				return
			}

			// Pass the request to the user-defined Gosp code.
			chdirOrAbort(sr.UserData.Filename)
			LaunchHTMLGenerator(p, conn, &sr.UserData)
		}(conn)
	}

	// Wait until all existing requests complete before we return.
	wg.Wait()
	return nil
}

// Parameters represents various parameters that control program operation.
type Parameters struct {
	SocketName       string        // Unix socket (filename) on which to listen for JSON requests
	FileName         string        // Name of a file from which to read a JSON request
	PluginName       string        // Name of a plugin file that provides a GospGenerateHTML function
	AutoKillTime     time.Duration // Amount of idle time after which the program should automatically exit
	HTTPHeaderType   string        // Format in which to output HTTP headers
	GospGenerateHTML func(*gosp.RequestData,
		gosp.Writer,
		gosp.Metadata) // Go Server Page as a function from a plugin
}

// ParseCommandLine parses the command line to fill in some of the fields of a
// Parameters struct.  It aborts the program on error.
func ParseCommandLine(p *Parameters) {
	// Parse the command line.
	wantVersion := flag.Bool("version", false, "Output the version number and exit")
	flag.StringVar(&p.SocketName, "socket", "",
		"Unix socket (filename) on which to listen for JSON requests")
	flag.StringVar(&p.FileName, "file", "",
		"File name from which to read a JSON request")
	flag.StringVar(&p.PluginName, "plugin", "",
		"Name of a plugin compiled from a Go Server Page by gosp2go")
	flag.DurationVar(&p.AutoKillTime, "max-idle", 5*time.Minute,
		"Maximum idle time before automatic server exit or 0s for infinite")
	flag.StringVar(&p.HTTPHeaderType, "http-headers", "mod_gosp",
		`HTTP header format: "mod_gosp", "raw", or "none"`)
	flag.Parse()

	// If requested, output the version number and exit.
	if *wantVersion {
		fmt.Fprintf(os.Stderr, "gosp-server (Go Server Pages) %s\n", Version)
		os.Exit(1)
	}

	// Validate the result.
	if p.PluginName == "" {
		notify.Fatal("--plugin is a required option")
	}
	if p.SocketName != "" && p.FileName != "" {
		notify.Fatal("--socket and --file are mutually exclusive")
	}
	switch p.HTTPHeaderType {
	case "mod_gosp", "raw", "none":
	default:
		notify.Fatalf("%q is not a valid argument to --http-headers", p.HTTPHeaderType)
	}
}

// LoadPlugin opens the plugin file and stores its GospGenerateHTML function as
// a program parameter.  It aborts the program on error.
func LoadPlugin(p *Parameters) {
	pl, err := plugin.Open(p.PluginName)
	if err != nil {
		notify.Fatal(err)
	}
	ggh, err := pl.Lookup("GospGenerateHTML")
	if err != nil {
		notify.Fatal(err)
	}
	var ok bool
	p.GospGenerateHTML, ok = ggh.(func(*gosp.RequestData, gosp.Writer, gosp.Metadata))
	if !ok {
		notify.Fatalf("the GospGenerateHTML function in %s has type %T instead of type %T",
			p.PluginName, ggh, p.GospGenerateHTML)
	}
}

func main() {
	// Initialize program parameters.
	var err error
	notify = log.New(os.Stderr, os.Args[0]+": ", 0)
	var p Parameters
	ParseCommandLine(&p)
	LoadPlugin(&p)

	// Process requests from a file or a socket, as directed by the user.
	os.Stdin = nil
	switch {
	case p.FileName != "":
		err = GospRequestFromFile(&p)
	case p.SocketName != "":
		err = StartServer(&p)
	default:
		LaunchHTMLGenerator(&p, os.Stdout, nil)
	}
	if err != nil {
		notify.Fatal(err)
	}
	return
}
