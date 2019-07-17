// gosp2go compiles a Go Server Page to ordinary Go code.
package main

import (
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"regexp"
	"strings"
)

// notify is used to output error messages.
var notify *log.Logger

// Parameters encapsulates the key program parameters.
type Parameters struct {
	InFileName  string // Name of a file from which to read Go Server Page HTML
	OutFileName string // Name of a file to which to write the Go or executable or HTML output
	Compile     bool   // true=compile the generated Go code
	Run         bool   // true=execute the generated Go code
}

// ParseCommandLine parses the command line and returns a set of
// parameters.  It aborts on error, first outputting a usage string.
func ParseCommandLine() *Parameters {
	// Prepare custom usage output.
	var p Parameters
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s [options] [input_file.gosp]\n\nThe following options are accepted:\n\n", os.Args[0])
		flag.PrintDefaults()
		os.Exit(1)
	}

	// Parse the command line.
	flag.StringVar(&p.OutFileName, "outfile", "-", `Name of output file ("-" = standard output); type depends on the other options specified`)
	flag.StringVar(&p.OutFileName, "o", "-", "Abbreviation of --outfile")
	flag.BoolVar(&p.Compile, "compile", false, "Compile the generated Go code to an executable program")
	flag.BoolVar(&p.Run, "run", false, "Compile and execute the generated Go code")
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
	if p.Compile && p.Run {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: --compile and --run are mutually exclusive.\n\n", os.Args[0])
		flag.Usage()
	}
	if p.Compile && (p.OutFileName == "" || p.OutFileName == "-") {
		fmt.Fprintf(flag.CommandLine.Output(), "%s: An output filename must be specified when --compile is used.\n\n", os.Args[0])
		flag.Usage()
	}
	return &p
}

// GospToGo converts a string representing a Go server page to a Go program.
func GospToGo(s string) string {
	// Parse each Gosp directive in turn.
	top := make([]string, 0, 1)   // Top-level Go code
	body := make([]string, 0, 16) // Main body Go code
	re := regexp.MustCompile(`<\?go:(top|block|expr)\s+((?:.|\n)*?)\?>\s*`)
	b := []byte(s)
	for {
		// Find the indexes of the first Gosp directive.
		idxs := re.FindSubmatchIndex(b)
		if idxs == nil {
			// No more directives.  Process any HTML text.
			if len(b) > 0 {
				body = append(body, fmt.Sprintf(`gospFmt.Printf("%%s", %q)`+"\n", b))
			}
			break
		}
		i0, i1, i2, i3 := idxs[2], idxs[3], idxs[4], idxs[5]
		dir := string(b[i0:i1])  // Directive
		code := string(b[i2:i3]) // Inner Go code

		// Extract HTML text preceding the Gosp code, if any.
		if i0 > 5 {
			body = append(body, fmt.Sprintf(`gospFmt.Printf("%%s", %q)`+"\n", b[:i0-5]))
		}

		// Extract Go code into either top or body.
		switch dir {
		case "top":
			// Top-level Go code.
			top = append(top, code)
			if i2 < i3 && b[i3-1] != '\n' {
				top = append(top, "\n")
			}
		case "block":
			// Zero or more statements.
			body = append(body, code)
			if i2 < i3 && b[i3-1] != '\n' {
				body = append(body, "\n")
			}
		case "expr":
			// A single Go expression.
			body = append(body, fmt.Sprintf(`gospFmt.Printf("%%v", %s)`+"\n", code))
		default:
			panic("Internal error parsing a Gosp directive")
		}

		// Continue with the text following the Gosp directive.
		b = b[idxs[1]:]
	}

	// Concatenate the accumulated strings into a Go program.
	all := make([]string, 0, len(top)+len(body)+10)
	all = append(all, "package main\n\n")
	all = append(all, `import gospFmt "fmt"`+"\n\n")
	all = append(all, top...)
	all = append(all, "\n")
	all = append(all, "func gospGenerateHTML() {\n")
	all = append(all, body...)
	all = append(all, "}\n")
	all = append(all, `
func main() {
  gospGenerateHTML()
}
`)
	return strings.Join(all, "")
}

// MakeTempGo writes a string of Go code to a temporary file and returns the
// name of that file.  The caller should delete the file when no longer needed.
// MakeTempGo aborts on error.
func MakeTempGo(goStr string) string {
	goFile, err := ioutil.TempFile("", "gosp-*.go")
	if err != nil {
		notify.Fatal(err)
	}
	fmt.Fprintln(goFile, goStr)
	goFile.Close()
	return goFile.Name()
}

// Compile compiles the generated Go code to a given filename.  It aborts on
// error.
func Compile(goStr, exeFn string) {
	goFn := MakeTempGo(goStr)
	defer os.Remove(goFn)
	cmd := exec.Command("go", "build", "-o", exeFn, goFn)
	cmd.Stdout = os.Stderr
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		notify.Fatal(err)
	}
}

// Run compiles and runs the generated Go code, sending its output to a given
// io.Writer.  It aborts on error.
func Run(goStr string, out io.Writer) {
	goFn := MakeTempGo(goStr)
	defer os.Remove(goFn)
	cmd := exec.Command("go", "run", goFn)
	cmd.Stdout = out
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		notify.Fatal(err)
	}
}

// SmartOpen opens a file for either reading or writing.  A filename of "-"
// indicates standard input/output.  The function aborts on error.
func SmartOpen(fn string, write bool) *os.File {
	var err error
	var file *os.File
	if write {
		// Writing
		switch {
		case fn == "":
			notify.Fatal("Empty output filename")
		case fn == "-":
			file = os.Stdout
		default:
			file, err = os.OpenFile(fn, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
		}
	} else {
		// Reading
		switch {
		case fn == "":
			notify.Fatal("Empty input filename")
		case fn == "-":
			file = os.Stdin
		default:
			file, err = os.Open(fn)
		}
	}
	if err != nil {
		notify.Fatal(err)
	}
	return file
}

func main() {
	// Parse the command line.
	var err error
	notify = log.New(os.Stderr, os.Args[0]+": ", 0)
	p := ParseCommandLine()

	// Open the input file.
	inFile := SmartOpen(p.InFileName, false)
	if p.InFileName == "-" {
		defer inFile.Close()
	}

	// Open the output file, unless we're only compiling.  In that case,
	// "go build" will write the output file itself.
	var outFile *os.File
	if !p.Compile {
		outFile = SmartOpen(p.OutFileName, true)
		if p.OutFileName == "-" {
			defer outFile.Close()
		}
	}

	// Read the entire input file and convert it to Go source code.
	in, err := ioutil.ReadAll(inFile)
	if err != nil {
		notify.Fatal(err)
	}
	goStr := GospToGo(string(in))

	// If Run is true, compile and run the Go program, outputting HTML.
	// Otherwise, if Compile is true, compile the Go program, outputting an
	// executable file.  Otherwise, output the Go source code itself.
	switch {
	case p.Run:
		// Run the Go program and output HTML.
		Run(goStr, outFile)
	case p.Compile:
		// Compile the Go program and output an executable file.
		Compile(goStr, p.OutFileName)
	default:
		// Output the Go program itself.
		fmt.Fprintln(outFile, goStr)
	}
}
