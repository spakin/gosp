// gosp2go compiles a Go Server Page to ordinary Go code.
package main

import (
	"fmt"
	"gosp"
	"io"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
)

// MaxIncludeDepth is the deepest we allow the file-inclusion tree to grow.
const MaxIncludeDepth = 10

// notify is used to output error messages.
var notify *log.Logger

// includeRe matches a file-inclusion directive.
var includeRe = regexp.MustCompile(`<\?go:include\s+(.*?)\s+\?>`)

// ProcessGospIncludes recursively processes <?go:include ... ?> blocks.  Only
// files lying within or below the including file's directory can be included.
// The function aborts on error.
func ProcessGospIncludes(p *Parameters, s []byte) []byte {
	return includeRe.ReplaceAllFunc(s, func(inc []byte) []byte {
		// Check that the parent is allowed to include the child.
		fn := string(includeRe.FindSubmatch(inc)[1])
		in, err := gosp.LiesInOrBelow(fn, p.DirStack[len(p.DirStack)-1])
		if err != nil {
			notify.Fatal(err)
		}
		if !in {
			notify.Fatalf("%s is not allowed to be included from directory %s",
				fn, p.DirStack[len(p.DirStack)-1])
		}

		// Open the child file and read its entire contents.
		f, err := os.Open(fn)
		if err != nil {
			notify.Fatal(err)
		}
		all, err := ioutil.ReadAll(f)
		if err != nil {
			notify.Fatal(err)
		}
		err = f.Close()
		if err != nil {
			notify.Fatal(err)
		}

		// Recursively process the child from the child's directory.
		p.PushDirectoryOf(fn)
		defer p.PopDirectory()
		return ProcessGospIncludes(p, all)
	})
}

// GospToGo converts a string representing a Go server page to a Go program.
func GospToGo(p *Parameters, s string) string {
	// Parse each Gosp directive in turn.
	top := make([]string, 0, 1)   // Top-level Go code
	body := make([]string, 0, 16) // Main body Go code
	re := regexp.MustCompile(`<\?go:(top|block|expr)\s+((?:.|\n)*?)\?>([\t ]*\n?)`)
	b := ProcessGospIncludes(p, []byte(s))
	for {
		// Find the indexes of the first Gosp directive.
		idxs := re.FindSubmatchIndex(b)
		if idxs == nil {
			// No more directives.  Process any page text.
			if len(b) > 0 {
				body = append(body, fmt.Sprintf(`gosp.Fprintf(gospOut, "%%s", %q)`+"\n", b))
			}
			break
		}
		i0, i1, i2, i3, i4, i5 := idxs[2], idxs[3], idxs[4], idxs[5], idxs[6], idxs[7]
		dir := string(b[i0:i1])    // Directive
		code := string(b[i2:i3])   // Inner Go code
		tSpace := string(b[i4:i5]) // Trailing white space

		// Extract any page text preceding the Gosp code.
		if i0 > 5 {
			body = append(body, fmt.Sprintf(`gosp.Fprintf(gospOut, "%%s", %q)`+"\n", b[:i0-5]))
		}

		// Extract Go code into either top or body.
		codeNeedsNL := i2 < i3 && b[i3-1] != '\n'
		switch dir {
		case "top":
			// Top-level Go code.
			if codeNeedsNL {
				top = append(top, code+"\n")
			} else {
				top = append(top, code)
			}
		case "block":
			// Zero or more statements.
			body = append(body, code)
			if codeNeedsNL {
				body = append(body, "\n")
			}
		case "expr":
			// A single Go expression.  In this case only, we
			// retain all trailing white space.
			body = append(body, fmt.Sprintf(`gosp.Fprintf(gospOut, "%%v%%s", %s, %q)`+"\n",
				strings.TrimSpace(code), tSpace))
		default:
			panic("Internal error parsing a Gosp directive")
		}

		// Continue with the text following the Gosp directive.
		b = b[idxs[1]:]
	}

	// Ensure we haven't violated the max-top constraint.
	if uint(len(top)) > p.MaxTop {
		notify.Fatalf("Too many go:top blocks (%d versus a maximum of %d)",
			len(top), p.MaxTop)
	}

	// Concatenate the accumulated strings into a Go program.
	all := make([]string, 0, len(top)+len(body)+10)
	all = append(all, header)
	all = append(all, top...)
	all = append(all, "\n")
	all = append(all, bodyBegin)
	all = append(all, body...)
	all = append(all, "}\n")
	return strings.Join(all, "")
}

// Build compiles the generated Go code to a given plugin filename.  It aborts
// on error.
func Build(p *Parameters, goStr, plugFn string) {
	goFn := MakeTempGo(goStr)
	defer os.Remove(goFn)
	cmd := exec.Command(p.GoCmd, "build", "--buildmode=plugin", "-o", plugFn, goFn)
	cmd.Stdout = os.Stderr
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		notify.Fatal(err)
	}
}

// Run compiles and runs the generated Go code, sending its output to a given
// io.Writer.  It aborts on error.
func Run(p *Parameters, goStr string, out io.Writer) {
	plug, err := ioutil.TempFile("", "gosp-*.go")
	if err != nil {
		notify.Fatal(err)
	}
	plugFn := plug.Name()
	Build(p, goStr, plugFn)
	defer os.Remove(plugFn)
	var cmd *exec.Cmd
	cmd = exec.Command("gosp-server", "-plugin", plugFn, "-http-headers", p.HTTPHeaderType)
	cmd.Stdout = out
	cmd.Stderr = os.Stderr
	err = cmd.Run()
	if err != nil {
		notify.Fatal(err)
	}
}

func main() {
	// Parse the command line.
	var err error
	notify = log.New(os.Stderr, os.Args[0]+": ", 0)
	p := ParseCommandLine()

	// Open the input file.
	inFile := SmartOpen(p.InFileName, false)
	if p.InFileName == "-" {
		p.PushDirectoryOf(string(filepath.Separator))
	} else {
		defer inFile.Close()
		p.PushDirectoryOf(p.InFileName)
	}

	// Open the output file, unless we're only compiling.  In that case,
	// "go build" will write the output file itself.
	var outFile *os.File
	if !p.Build {
		outFile = SmartOpen(p.OutFileName, true)
		if p.OutFileName != "-" {
			defer outFile.Close()
		}
	}

	// Read the entire input file and convert it to Go source code.
	in, err := ioutil.ReadAll(inFile)
	if err != nil {
		notify.Fatal(err)
	}
	goStr := GospToGo(p, string(in))
	err = p.ValidateImports(goStr)
	if err != nil {
		notify.Fatal(err)
	}

	// If Run is true, compile and run the Go program, outputting the page
	// data (typically HTML).  Otherwise, if Build is true, compile the Go
	// program, outputting an executable file.  Otherwise, output the Go
	// source code itself.
	switch {
	case p.Run:
		// Run the Go program and output a Web page.
		Run(p, goStr, outFile)
	case p.Build:
		// Compile the Go program and output an executable file.
		Build(p, goStr, p.OutFileName)
	default:
		// Output the Go program itself.
		fmt.Fprintln(outFile, goStr)
	}
}
