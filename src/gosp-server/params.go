// This file defines functions that handle gosp-server's execution parameters.

package main

import (
	"flag"
	"fmt"
	"gosp"
	"io"
	"os"
	"time"
)

// A PageGenerator is a function that takes request information and generates
// a Web page (typically in HTML format) and associated metadata.
type PageGenerator func(*gosp.RequestData, gosp.Writer, gosp.Metadata)

// A MetadataWriter reads HTTP metadata from a channel and writes it in some
// format to an io.Writer.
type MetadataWriter func(gospOut io.Writer, meta chan gosp.KeyValue) string

// Parameters represents various parameters that control program operation.
type Parameters struct {
	SocketName       string         // Unix socket (filename) on which to listen for JSON requests
	FileName         string         // Name of a file from which to read a JSON request
	PluginName       string         // Name of a plugin file that provides a GospGenerateHTML function
	AutoKillTime     time.Duration  // Amount of idle time after which the program should automatically exit
	WriteMetadata    MetadataWriter // Function that writes HTTP metadata in some particular format
	GospGenerateHTML PageGenerator  // Go Server Page as a function from a plugin
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
	hType := flag.String("http-headers", "mod_gosp",
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
	switch *hType {
	case "mod_gosp":
		p.WriteMetadata = writeModGospMetadata
	case "raw":
		p.WriteMetadata = writeRawMetadata
	case "none":
		p.WriteMetadata = writeNoMetadata
	default:
		notify.Fatalf("%q is not a valid argument to --http-headers", *hType)
	}
}
