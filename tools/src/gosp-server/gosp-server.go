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
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

// autoKillTime represents the maximum time in minutes a Gosp server is allowed
// to be idle before exiting or 0 for infinite time.  This can be overridden
// with go's -X linker option.
var autoKillTime = "0"

// autoKillMinutes is duration version of the string autoKillTime.
var autoKillMinutes time.Duration

// LaunchHTMLGenerator starts GospGenerateHTML in a separate goroutine and
// waits for it to finish.
func LaunchHTMLGenerator(gospOut io.Writer, gospReq *gosp.Request) {
	// Spawn GospGenerateHTML, giving it a buffer in which to write HTML
	// and a channel in which to send metadata.
	html := bytes.NewBuffer(nil)
	meta := make(chan gosp.KeyValue, 5)
	go GospGenerateHTML(gospReq, html, meta)

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

// chdirOrPanic changes to the directory containing the Go Server Page.  It panics
// on error.
func chdirOrPanic(req *gosp.Request) {
	if req.Filename == "" {
		return
	}
	err := os.Chdir(filepath.Dir(req.Filename))
	if err != nil {
		panic(err)
	}
}

// GospRequestFromFile reads a gosp.Request from a named JSON file and passes
// this to LaunchHTMLGenerator.
func GospRequestFromFile(fn string) error {
	f, err := os.Open(fn)
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
	chdirOrPanic(&gr)
	LaunchHTMLGenerator(os.Stdout, &gr)
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
func StartServer(fn string) error {
	// Server code should write only to the io.Writer it's given and not
	// read at all.
	os.Stdin.Close()
	os.Stdout.Close()

	// Listen on the named Unix-domain socket.
	sock, err := filepath.Abs(fn)
	if err != nil {
		return err
	}
	_ = os.Remove(sock) // It's not an error if the socket doesn't exist.
	ln, err := net.Listen("unix", sock)
	if err != nil {
		return err
	}

	// Exit automatically after gospAutoKill minutes of no activity.
	var killClk *time.Timer
	if autoKillMinutes > 0 {
		killClk = time.AfterFunc(autoKillMinutes*time.Minute, func() {
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
		ResetKillClock(killClk, autoKillMinutes*time.Minute)
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
			chdirOrPanic(&gr)
			LaunchHTMLGenerator(conn, &gr)
		}(conn)
	}

	// Wait until all existing requests complete before we return.
	wg.Wait()
	return nil
}

// Temporary
func GospGenerateHTML(*gosp.Request, *bytes.Buffer, chan<- gosp.KeyValue) {
}

func main() {
	akm, err := strconv.Atoi(autoKillTime)
	if err != nil {
		panic(err)
	}
	autoKillMinutes = time.Duration(akm)
	os.Stdin = nil
	sock := flag.String("socket", "", "Unix socket (filename) on which to listen for JSON code")
	file := flag.String("file", "", "File name from which to read JSON code")
	flag.Parse()
	switch {
	case *file != "":
		err = GospRequestFromFile(*file)
	case *sock != "":
		err = StartServer(*sock)
	default:
		LaunchHTMLGenerator(os.Stdout, nil)
	}
	if err != nil {
		panic(err)
	}
	return
}