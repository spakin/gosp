---
title: Helper package
---

Helper package
==============

Go Server Pages defines a `gosp` Go package that serves two purposes:

1. It provides all the functionality needed by the generated `GospGenerateHTML` function to produce a Web page.

2. It exposes a few key functions and data types to Go Server Pages so even a page that is not authorized to import any Go packages whatsoever (see [Configuring Go Server Pages](configure.md)) can still produce Web-page data and can still open for reading any file that lies in the same directory or a subdirectory.

The source code for the helper package lies in the [`gosp`](https://github.com/spakin/gosp/tree/master/tools/src/gosp) directory.  Look there for the definition of `gosp.RequestData`, the set of all client and server data passed in from the Apache module for use by a Go Server Page.
