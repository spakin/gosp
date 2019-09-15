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
	"sync"
	"sync/atomic"
	"time"
)

// notify is used to output error messages.
var notify *log.Logger

// LaunchHTMLGenerator starts GospGenerateHTML in a separate goroutine and
// waits for it to finish.
func LaunchHTMLGenerator(p *Parameters, gospOut io.Writer, gospReq *gosp.Request) {
	// Spawn GospGenerateHTML, giving it a buffer in which to write HTML
	// and a channel in which to send metadata.
	html := bytes.NewBuffer(nil)
	meta := make(chan gosp.KeyValue, 5)
	go p.GospGenerateHTML(gospReq, html, meta)

	// Read metadata from GospGenerateHTML until no more remains.
	okStr := fmt.Sprint(http.StatusOK)
	status := okStr
	for kv := range meta {
		switch kv.Key {
		case "mime-type", "http-status", "header-field", "keep-alive", "debug-message":
			fmt.Fprintf(gospOut, "%s %s\n", kv.Key, kv.Value)
		}

		// Keep track of the current HTTP status code.
		if kv.Key == "http-status" {
			status = kv.Value
		}
	}
	fmt.Fprintln(gospOut, "end-header")
	if status == okStr {
		fmt.Fprint(gospOut, html)
	}
}

// chdirOrAbort changes to the directory containing the Go Server Page.  It
// aborts on error.
func chdirOrAbort(req *gosp.Request) {
	if req.Filename == "" {
		return
	}
	err := os.Chdir(filepath.Dir(req.Filename))
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
	var gr gosp.Request
	err = dec.Decode(&gr)
	if err != nil {
		return err
	}
	chdirOrAbort(&gr)
	LaunchHTMLGenerator(p, os.Stdout, &gr)
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
	os.Stdin.Close()
	os.Stdout.Close()

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
			conn.SetDeadline(time.Now().Add(10 * time.Second))
			dec := json.NewDecoder(conn)
			var gr gosp.Request
			err = dec.Decode(&gr)
			if err != nil {
				return
			}

			// If we were asked to exit, send back our PID, notify
			// our parent, and establish a dummy connection to
			// flush the pipeline.
			if gr.ExitNow {
				fmt.Fprintf(conn, "gosp-pid %d\n", os.Getpid())
				atomic.StoreInt32(&done, 1)
				c, err := net.Dial("unix", sock)
				if err == nil {
					c.Close()
				}
				return
			}

			// Pass the request to the user-defined Gosp code.
			chdirOrAbort(&gr)
			LaunchHTMLGenerator(p, conn, &gr)
		}(conn)
	}

	// Wait until all existing requests complete before we return.
	wg.Wait()
	return nil
}

// UserCode is the type of a function compiled from a Go Server Page into a Go
// plugin.
type UserCode func(*gosp.Request, *bytes.Buffer, chan<- gosp.KeyValue)

// Parameters represents various parameters that control program operation.
type Parameters struct {
	SocketName       string        // Unix socket (filename) on which to listen for JSON requests
	FileName         string        // Name of a file from which to read a JSON request
	PluginName       string        // Name of a plugin file that provides a GospGenerateHTML function
	AutoKillTime     time.Duration // Amount of idle time after which the program should automatically exit
	GospGenerateHTML func(*gosp.Request,
		gosp.Writer,
		chan<- gosp.KeyValue) // Go Server Page as a function from a plugin
}

// ParseCommandLine parses the command line to fill in some of the fields of a
// Parameters struct.  It aborts the program on error.
func ParseCommandLine(p *Parameters) {
	// Parse the command line.
	flag.StringVar(&p.SocketName, "socket", "", "Unix socket (filename) on which to listen for JSON requests")
	flag.StringVar(&p.FileName, "file", "", "File name from which to read a JSON request")
	flag.StringVar(&p.PluginName, "plugin", "", "Name of a plugin compiled from a Go Server Page by gosp2go")
	flag.DurationVar(&p.AutoKillTime, "max-idle", 5*time.Minute, "Maximum idle time before automatic server exit or 0s for infinite")
	flag.Parse()

	// Validate the result.
	if p.PluginName == "" {
		notify.Fatal("--plugin is a required option")
	}
	if p.SocketName != "" && p.FileName != "" {
		notify.Fatal("--socket and --file are mutually exclusive")
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
	p.GospGenerateHTML, ok = ggh.(func(*gosp.Request, gosp.Writer, chan<- gosp.KeyValue))
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
