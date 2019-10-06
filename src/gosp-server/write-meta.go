// This file defines various functions of type MetadataWriter.

package main

import (
	"fmt"
	"gosp"
	"io"
	"strings"
	"unicode"
)

// writeRawMetadata is a helper routine for LaunchPageGenerator that writes
// HTTP metadata in raw HTTP format.  It returns an HTTP status as a string.
func writeRawMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	// Define default header values.
	contentType := "text/html"
	headers := make([]string, 0, 3)
	status := okStr

	// Read metadata from GospGeneratePage until no more remains.
	for kv := range meta {
		switch kv.Key {
		case "mime-type":
			contentType = kv.Value
		case "http-status":
			status = kv.Value
		case "header-field":
			headers = append(headers, kv.Value)
		}
	}

	// Output the metadata.
	fmt.Fprintf(gospOut, "Content-type: %s\n", contentType)
	for _, h := range headers {
		fmt.Fprintln(gospOut, h)
	}
	fmt.Fprintln(gospOut, "")
	return status
}

// writeNoMetadata is a helper routine for LaunchPageGenerator that discards
// all metadata.  It might be used when postprocessing the output of CGI
// scripts, which already output an HTTP header.  writeNoMetadata returns an
// HTTP status as a string.
func writeNoMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	status := okStr
	for kv := range meta {
		if kv.Key == "http-status" {
			status = kv.Value
		}
	}
	return status
}

// sanitizeString is a helper routine for writeModGospMetadata that replaces a
// string's newlines, tabs, and other whitespace characters with ordinary
// spaces.
func sanitizeString(s string) string {
	return strings.Map(func(r rune) rune {
		if unicode.IsSpace(r) {
			return ' '
		}
		return r
	}, s)
}

// writeModGospMetadata is a helper routine for LaunchPageGenerator that writes
// HTTP metadata in the format expected by the Gosp Apache module.  It returns
// an HTTP status as a string.
func writeModGospMetadata(gospOut io.Writer, meta chan gosp.KeyValue) string {
	// Read metadata from GospGeneratePage until no more remains.
	status := okStr
	for kv := range meta {
		switch kv.Key {
		case "mime-type", "http-status", "header-field", "keep-alive", "error-message", "debug-message":
			k := sanitizeString(kv.Key)
			v := sanitizeString(kv.Value)
			fmt.Fprintln(gospOut, k, v)
		}

		// Keep track of the current HTTP status code.
		if kv.Key == "http-status" {
			status = kv.Value
		}
	}
	fmt.Fprintln(gospOut, "end-header")
	return status
}
