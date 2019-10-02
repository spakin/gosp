---
title: Page compiler
parent: Implementation
nav_order: 2
---

Page compiler
=============

The purpose of `gosp2go`, the Go Server Pages page compiler, is to compile a Go Server Page from HTML with embedded Go code to a Go function that both executes code and outputs HTML.  (We write "HTML" here, but a Go Server Page can in fact use any file format.)  For example, the HTML fragment
```html
<p>
  Let's sing: <q>Na<?go:expr strings.Repeat(" na", 7) ?>,
  <?go:expr strings.Repeat(" na", 8) ?>, Batman!</q>
</p>
```
compiles to the Go fragment,
```go
gosp.Fprintf(gospOut, "%s", "<p>\n  Let's sing: <q>Na")
gosp.Fprintf(gospOut, "%v%s", strings.Repeat(" na", 7), "")
gosp.Fprintf(gospOut, "%s", ",\n  ")
gosp.Fprintf(gospOut, "%v%s", strings.Repeat(" na", 8), "")
gosp.Fprintf(gospOut, "%s", ", Batman!</q>\n</p>\n")
```

This code fragment is written as part of a `GospGenerateHTML` function of type
```go
func(gospReq *gosp.RequestData, gospOut gosp.Writer, gospMeta gosp.Metadata)
```
See [`boilerplate.go`](https://github.com/spakin/gosp/tree/master/src/gosp2go/boilerplate.go) for the latest definition of the boilerplate code that wraps the code generated from the contents of the Go Server Page.  The only package the boilerplate code imports is `gosp`.

The source code for the compiler lies in the [`gosp2go`](https://github.com/spakin/gosp/tree/master/src/gosp2go) directory.  `gosp2go`'s default behavior is to produce Go source code, which can be useful for troubleshooting a Go Server Page.  When invoked from the Go Server Pages Apache module, `gosp2go` is instructed via the `--build` option not only to generate Go code but also to compile the result to a shared object (plugin).  `gosp2go` can also be invoked with the `--run` option to compile, generate a plugin, and run the plugin using `gosp-server`.  This can be useful for post-processing the output of a [CGI](https://en.wikipedia.org/wiki/Common_Gateway_Interface) script that generates a Go Server Page instead of ordinary HTML.
