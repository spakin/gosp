// This file defines some utility functions needed to convert Go Server Pages
// to Go programs.

package main

import (
	"fmt"
	"go/parser"
	"go/token"
	"io/ioutil"
	"os"
	"path/filepath"
)

// MakeTempGo writes a string of Go code to main.go in a temporary directory
// and returns the name of that directory.  The caller should delete the
// directory when no longer needed.  MakeTempGo aborts on error.
func MakeTempGo(goStr string) string {
	// Create a gosp-* directory.
	goDir, err := ioutil.TempDir("", "gosp-*")
	if err != nil {
		notify.Fatal(err)
	}

	// Create main.go in the temporary directory.
	goFile, err := os.Create(filepath.Join(goDir, "main.go"))
	if err != nil {
		notify.Fatal(err)
	}
	fmt.Fprintln(goFile, goStr)
	err = goFile.Close()
	if err != nil {
		notify.Fatal(err)
	}
	return goDir
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
