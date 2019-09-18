// This file defines functions that handle gosp2go's execution parameters.

package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
)

// Parameters encapsulates the key program parameters.
type Parameters struct {
	InFileName     string   // Name of a file from which to read Go Server Page HTML
	OutFileName    string   // Name of a file to which to write the Go code or plugin or HTML output
	Build          bool     // true=compile the generated Go code
	Run            bool     // true=execute the generated Go code
	MaxTop         uint     // Maximum number of go:top blocks allowed per page
	GoCmd          string   // Go compiler executable (e.g., "/usr/bin/go")
	DirStack       []string // Stack of directories to which we chdir
	HttpHeaderType string   // Format in which to output HTTP headers
}

// PushDirectoryOf switches to the parent directory of a given file.  It aborts
// on error.
func (p *Parameters) PushDirectoryOf(fn string) {
	dir, err := filepath.Abs(filepath.Dir(fn))
	if err != nil {
		notify.Fatal(err)
	}
	err = os.Chdir(dir)
	if err != nil {
		notify.Fatal(err)
	}
	p.DirStack = append(p.DirStack, dir)
	if len(p.DirStack) > MaxIncludeDepth+1 { // +1 for the initial directory.
		notify.Fatal(fmt.Errorf("Inclusion depth exceeded the maximum of %d", MaxIncludeDepth))
	}
}

// PopDirectory returns to the previous directory we were in.  It aborts on
// error.
func (p *Parameters) PopDirectory() {
	nds := len(p.DirStack)
	dir := p.DirStack[nds-1]
	err := os.Chdir(dir)
	if err != nil {
		notify.Fatal(err)
	}
	p.DirStack = p.DirStack[:nds-1]
}

// ParseCommandLine parses the command line and returns a set of
// parameters.  It aborts on error, first outputting a usage string.
func ParseCommandLine() *Parameters {
	// Prepare custom usage output.
	var p Parameters
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), `Usage: %s [options] [input_file.gosp]

The following options are accepted:

  -b, --build  Compile the generated Go code to a plugin

  -r, --run    Both compile and execute the generated Go code

  -t NUM, --max-top=NUM
               Allow at most NUM <?go:top ... ?> blocks per file
               [default: 1]

  -o FILE, --output=FILE
               Write output to file FILE ("-" = standard output, the
               default); the file type depends on whether --build,
               --run, or neither is also specified

  -H, --raw-headers
               Output raw HTTP headers instead of specially formatted headers
               for the Gosp Apache module

  -g FILE, --go=FILE
               Use FILE as the Go compiler [default: "go"]

`, os.Args[0])
		os.Exit(1)
	}

	// Parse the command line.
	flag.StringVar(&p.OutFileName, "outfile", "-", `Name of output file ("-" = standard output); type depends on the other options specified`)
	flag.StringVar(&p.OutFileName, "o", "-", "Abbreviation of --outfile")
	flag.BoolVar(&p.Build, "build", false, "Compile the generated Go code to an executable program")
	flag.BoolVar(&p.Build, "b", false, "Abbreviation of --build")
	flag.BoolVar(&p.Run, "run", false, "Compile and execute the generated Go code")
	flag.BoolVar(&p.Run, "r", false, "Abbreviation of --run")
	flag.UintVar(&p.MaxTop, "max-top", 1, "Maximum number of go:top blocks per file")
	flag.UintVar(&p.MaxTop, "t", 1, "Abbreviation of --max-top")
	flag.StringVar(&p.GoCmd, "go", "go", "Name of the Go executable")
	flag.StringVar(&p.GoCmd, "g", "go", "Abbreviation of --go")
	flag.StringVar(&p.HttpHeaderType, "http-headers", "mod_gosp", "HTTP header format to request from gosp-server")
	flag.StringVar(&p.HttpHeaderType, "H", "mod_gosp", "Abbreviation of --raw-headers")
	flag.Parse()

	// Check the parameters for self-consistency.
	switch flag.NArg() {
	case 0:
		p.InFileName = "-"
	case 1:
		p.InFileName = flag.Arg(0)
	default:
		flag.Usage()
	}
	if p.Build && p.Run {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: --build and --run are mutually exclusive.\n\n", os.Args[0])
		flag.Usage()
	}
	if p.Build && (p.OutFileName == "" || p.OutFileName == "-") {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: An output filename must be specified when --build is used.\n\n", os.Args[0])
		flag.Usage()
	}
	return &p
}
