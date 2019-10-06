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

// LoadPlugin opens the plugin file and stores its GospGenerateHTML function as
// a program parameter.  It aborts the program on error.
func LoadPlugin(p *Parameters) {
	pl, err := plugin.Open(p.PluginName)
	if err != nil {
		notify.Fatal(err)
	}
	gghSym, err := pl.Lookup("GospGenerateHTML")
	if err != nil {
		notify.Fatal(err)
	}
	ggh, ok := gghSym.(func(*gosp.RequestData, gosp.Writer, gosp.Metadata))
	if !ok {
		notify.Fatalf("the GospGenerateHTML function in %s has type %T instead of type %T",
			p.PluginName, ggh, p.GospGenerateHTML)
	}
	p.GospGenerateHTML = PageGenerator(ggh)
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
