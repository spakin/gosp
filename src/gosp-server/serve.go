// This file serves page requests provided via either a socket or file.

package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"gosp"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"sync"
	"sync/atomic"
	"time"
)

// A ServiceRequest is a request for service sent to us by the Web server.
type ServiceRequest struct {
	UserData gosp.RequestData // Data to pass to the user's code
	GetPID   bool             // If true, respond with our process ID
	ExitNow  bool             // If true, shut down the program cleanly
}

// okStr represents an HTTP success code as a string.
var okStr = fmt.Sprint(http.StatusOK)

// LaunchPageGenerator starts GospGeneratePage in a separate goroutine and
// waits for it to finish.
func LaunchPageGenerator(p *Parameters, gospOut io.Writer, gospReq *gosp.RequestData) {
	// Spawn GospGeneratePage, giving it a buffer in which to write the
	// page data and a channel in which to send metadata.
	html := bytes.NewBuffer(nil)
	meta := make(chan gosp.KeyValue, 5)
	go p.GospGeneratePage(gospReq, html, meta)

	// Tell the server what we think its JSON request is.
	meta <- gosp.KeyValue{
		Key:   "debug-message",
		Value: sanitizeString(fmt.Sprintf("Handling %#v", gospReq)),
	}

	// Read metadata from GospGeneratePage until no more remains.
	status := p.WriteMetadata(gospOut, meta)

	// Write the generated page, but only on success.
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
// this to LaunchPageGenerator.
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
	LaunchPageGenerator(p, os.Stdout, &sr.UserData)
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
// LaunchPageGenerator to respond to the request.  The server terminates
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

			// If we were sent a PID request, send back our PID in
			// response.  Ignore the rest of the request.
			if sr.GetPID {
				fmt.Fprintf(conn, "gosp-pid %d\n", os.Getpid())
				return
			}

			// Pass the request to the user-defined Gosp code.
			chdirOrAbort(sr.UserData.Filename)
			LaunchPageGenerator(p, conn, &sr.UserData)
		}(conn)
	}

	// Wait until all existing requests complete before we return.
	wg.Wait()
	return nil
}
