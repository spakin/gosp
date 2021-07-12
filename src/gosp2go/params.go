// This file defines functions that handle gosp2go's execution parameters.

package main

import (
	"errors"
	"flag"
	"fmt"
	"os"
	"sort"
	"strings"
)

// Version defines the Go Server Pages version number.  This should be
// overridden by the Makefile.
var Version = "?.?.?"

// Parameters encapsulates the key program parameters.
type Parameters struct {
	InFileName     string                // Name of a file from which to read the Go Server Page proper
	OutFileName    string                // Name of a file to which to write the Go code or plugin or Web page
	Build          bool                  // true=compile the generated Go code
	Run            bool                  // true=execute the generated Go code
	MaxTop         uint                  // Maximum number of go:top blocks allowed per page
	GoCmd          string                // Go compiler executable (e.g., "/usr/bin/go")
	DirStack       []string              // Stack of directories to which we chdir
	AllowedImports ImportSet             // Set of packages the Go code is allowe to import
	GospServerArgs []string              // Additional arguments to pass to gosp-server
	ModRepls       ModuleReplacementList // List of module replacements to write to generated go.mod files
}

// An ImportSet represents a set of package names.  The Boolean value is always
// true and is used to simplify membership testing.
type ImportSet map[string]bool

// NewImportSet returns a new ImportSet that allows no packages.
func NewImportSet() ImportSet {
	return make(map[string]bool, 16) // Arbitrary initial size
}

// String returns an ImportSet as a comma-separated list of strings.
func (s *ImportSet) String() string {
	str := make([]string, 0, len(*s))
	for k := range *s {
		str = append(str, k)
	}
	sort.Strings(str)
	return strings.Join(str, ",")
}

// Set assigns in turn each package named in a comma-separated list to an
// ImportSet.  "ALL" indicates that all packages are allowed.  "NONE" indicates
// that no packages are allowed.
func (s *ImportSet) Set(lst string) error {
	if strings.TrimSpace(lst) == "" {
		return errors.New("Argument must be non-empty")
	}
	for _, pkg := range strings.Split(lst, ",") {
		pkg = strings.TrimSpace(pkg)
		switch pkg {
		case "":
			return fmt.Errorf("Empty element in list %q", lst)
		case "NONE":
			*s = NewImportSet()
		case "ALL":
			*s = NewImportSet()
			(*s)["ALL"] = true
		default:
			if !(*s)["ALL"] {
				(*s)[pkg] = true
			}
		}
	}
	return nil
}

// A ModuleReplacement represents a single replacement to write to go.mod.
type ModuleReplacement struct {
	Module string // Module to replace
	Path   string // Path name to replace it with
}

// ModuleReplacementList is a list of replacements to write to go.mod.
type ModuleReplacementList []ModuleReplacement

// String pretty-prints a ModuleReplacementList.
func (r *ModuleReplacementList) String() string {
	if r == nil {
		return ""
	}
	repls := make([]string, 0, 3)
	for _, rr := range *r {
		repls = append(repls, fmt.Sprintf("%s,%q", rr.Module, rr.Path))
	}
	return strings.Join(repls, ",")
}

// Set appends a new replacement to a ModuleReplacementList.
func (r *ModuleReplacementList) Set(s string) error {
	ss := strings.Split(s, ",")
	if len(ss) != 2 {
		return fmt.Errorf("failed to parse %q as \"<module>,<path>\"", s)
	}
	*r = append(*r, ModuleReplacement{
		Module: ss[0],
		Path:   ss[1],
	})
	return nil
}

// showUsage displays command-line usage in a human-friendly format then exits
// with an error code.  It is intended to be assigned to flag.Usage.
func showUsage() {
	fmt.Fprintf(flag.CommandLine.Output(), `Usage: %s [<options>] [input_file.gosp] [-- [<gosp-server options>]]

The following options are accepted:

  -b, --build  Compile the generated Go code to a plugin

  -r, --run    Both compile and, using gosp-sever, execute the generated
               Go code

  -t NUM, --max-top=NUM
               Allow at most NUM <?go:top ... ?> blocks per file
               [default: 1]

  -o FILE, --output=FILE
               Write output to file FILE ("-" = standard output, the
               default); the file type depends on whether --build,
               --run, or neither is also specified

  -g FILE, --go=FILE
               Use FILE as the Go compiler [default: "go"]

  -a LIST, --allowed=LIST
               Specify a comma-separated list of allowed Go imports; if "ALL"
               (the default), allow all imports; if "NONE", allow no imports
  --replace MODULE,PATH
               Indicate a module replacement to write to go.mod

  --version    Output the gosp2go version number and exit

  --help       Output gosp2go usage information and exit
`, os.Args[0])
	os.Exit(1)
}

// checkParams is a helper function for ParseCommandLine that ensures that the
// given parameters are self-consistent.  It gives a usage message and aborts
// if not.
func checkParams(p *Parameters) {
	if p.Build && p.Run {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: --build and --run are mutually exclusive.\n\n", os.Args[0])
		flag.Usage()
	}
	if p.Build && (p.OutFileName == "" || p.OutFileName == "-") {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: An output filename must be specified when --build is used.\n\n", os.Args[0])
		flag.Usage()
	}
}

// assignGospServerArgs prepares to pass any remaining arguments to gosp-server.
func assignGospServerArgs(p *Parameters) {
	p.GospServerArgs = flag.Args()
	p.InFileName = "-"
	if len(p.GospServerArgs) > 0 {
		// If the first extra argument is not an option, it designates
		// the name of the input file.
		first := p.GospServerArgs[0]
		if len(first) > 0 && first[0] != '-' {
			p.InFileName = first
			p.GospServerArgs = p.GospServerArgs[1:]
		}
	}
	if len(p.GospServerArgs) > 0 && p.GospServerArgs[0] == "--" {
		// Skip the "--" separator.
		p.GospServerArgs = p.GospServerArgs[1:]
	}
	for _, opt := range p.GospServerArgs {
		if len(opt) > 0 && opt[0] != '-' {
			// Accept only options, not more filenames.
			flag.Usage()
		}
	}
}

// ParseCommandLine parses the command line and returns a set of
// parameters.  It aborts on error, first outputting a usage string.
func ParseCommandLine() *Parameters {
	// Parse the command line.
	var p Parameters
	flag.Usage = showUsage
	wantVersion := flag.Bool("version", false, "Output the version number and exit")
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
	p.AllowedImports = NewImportSet()
	p.AllowedImports["PLACEHOLDER_ALL"] = true // Converted to ALL if --allowed was never used
	flag.Var(&p.AllowedImports, "allowed", "Comma-separated list of allowed Go imports")
	flag.Var(&p.AllowedImports, "a", "Abbreviation of --allowed")
	flag.Var(&p.ModRepls, "replace", `Module replacement to write to go.mod, expressed as "<module>,<path>"`)
	flag.Parse()
	assignGospServerArgs(&p)

	// If requested, output the version number and exit.
	if *wantVersion {
		fmt.Fprintf(os.Stderr, "gosp2go (Go Server Pages) %s\n", Version)
		os.Exit(1)
	}

	// Replace PLACEHOLDER_ALL with ALL if it's the only package allowed.
	if p.AllowedImports["PLACEHOLDER_ALL"] {
		if len(p.AllowedImports) == 1 {
			// --allowed was not specified.  Default to --allowed=ALL.
			p.AllowedImports = NewImportSet()
			p.AllowedImports["ALL"] = true
		}
		delete(p.AllowedImports, "PLACEHOLDER_ALL")
	}

	// Check the parameters for self-consistency.
	checkParams(&p)
	return &p
}
