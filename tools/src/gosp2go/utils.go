// This file defines some utility functions needed to convert Go Server Pages
// to Go programs.

package main

import (
	"fmt"
	"go/parser"
	"go/token"
	"io/ioutil"
	"os"
)

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

// ValidateImports returns an error if the given string of Go code attempts to
// import a package that is not in the allowed list.
func (p *Parameters) ValidateImports(code string) error {
	// If all packages are allowed, we don't need to check individual
	// imports.
	if p.AllowedImports["ALL"] {
		return nil
	}

	// Parse the list of imports from the input string.
	inName := p.InFileName
	if inName == "-" {
		inName = "<standard input>"
	}
	f, err := parser.ParseFile(token.NewFileSet(), p.InFileName, code, parser.ImportsOnly)
	if err != nil {
		return err
	}

	// Consider each import in turn.
	for _, s := range f.Imports {
		imp := s.Path.Value
		imp = imp[1 : len(imp)-1] // Remove surrounding quotes
		if imp == "gosp" {
			continue // The gosp package is implicitly allowed.
		}
		if !p.AllowedImports[imp] {
			// Not found -- issue a helpful error message.
			return fmt.Errorf("%q is not in the list of approved packages for %s (%q)",
				imp, inName, p.AllowedImports.String())
		}
	}
	return nil
}
