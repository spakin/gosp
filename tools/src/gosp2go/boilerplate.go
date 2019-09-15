// This file defines boilerplate code that forms part of every
// generated Go file.

package main

// header is included at the top of every generated Go file.
var header = `// This file was generated by gosp2go.

package main

import "gosp"

`

// body begins the function that was converted from a Go Server Page to Go.
var bodyBegin = `// GospGenerateHTML represents the user's Go Server page, converted to Go.
func GospGenerateHTML(gospReq *gosp.RequestData, gospOut gosp.Writer, gospMeta chan<- gosp.KeyValue) {
	// On exit, close the metadata channel.  If the user's code panicked,
	// change the return code to "internal server error".
	defer func() {
		if r := recover(); r != nil {
			gosp.SetHttpStatus(gospMeta, gosp.StatusInternalServerError)
		}
		close(gospMeta)
	}()

	// Provide functions for passing metadata back to the Web server.
	GospSetHttpStatus := func(s int) { gosp.SetHttpStatus(gospMeta, s) }
	GospSetMimeType := func(mt string) { gosp.SetMimeType (gospMeta, mt) }
	GospSetHeaderField := func(k, v string, repl bool) {
		gosp.SetHeaderField(gospMeta, k, v, repl)
	}
	_, _, _ = GospSetHttpStatus, GospSetMimeType, GospSetHeaderField

	// Express the Gosp page in Go.
`
