// gosp-server launches a server process that accepts requests on a local
// socket and sends back metadata followed by data (e.g., HTML).  As an option,
// it can generate the data directly as a one-shot execution rather than from a
// persistent server process.
package main

import (
	"gosp"
	"log"
	"os"
	"plugin"
)

// Version defines the Go Server Pages version number.  It should be overridden
// by the Makefile.
var Version = "?.?.?"

// notify is used to output error messages.
var notify *log.Logger

// LoadPlugin opens the plugin file and stores its GospGeneratePage function as
// a program parameter.  It aborts the program on error.
func LoadPlugin(p *Parameters) {
	pl, err := plugin.Open(p.PluginName)
	if err != nil {
		notify.Fatal(err)
	}
	ggpSym, err := pl.Lookup("GospGeneratePage")
	if err != nil {
		notify.Fatal(err)
	}
	ggp, ok := ggpSym.(func(*gosp.RequestData, gosp.Writer, gosp.Metadata))
	if !ok {
		notify.Fatalf("the GospGeneratePage function in %s has type %T instead of type %T",
			p.PluginName, ggpSym, ggp)
	}
	p.GospGeneratePage = PageGenerator(ggp)
}

func main() {
	// Initialize program parameters.
	var err error
	notify = log.New(os.Stderr, os.Args[0]+": ", 0)
	var p Parameters
	ParseCommandLine(&p)
	LoadPlugin(&p)
	if p.DryRun {
		os.Exit(0)
	}

	// Process requests from a file or a socket, as directed by the user.
	os.Stdin = nil
	switch {
	case p.FileName != "":
		err = GospRequestFromFile(&p)
	case p.SocketName != "":
		err = StartServer(&p)
	default:
		LaunchPageGenerator(&p, os.Stdout, nil)
	}
	if err != nil {
		notify.Fatal(err)
	}
	return
}
