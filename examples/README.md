Go Server Pages examples
========================

This directory provides a number of examples of the types of tasks one can accomplish by embedding Go code on a Web page.  In order to actually test these examples, you will need to install and configure Go Server Pages.  [See the Go Server Pages documentation](https://gosp.pakin.org/) for instructions.

The examples, sorted roughly from most basic to most advanced, are described below.  Note that many of these use **import** to import various Go packages.  The Web server needs to be configured to authorize this action, as described in [Configuring Go Server Pages](https://gosp.pakin.org/configure.html).

* [do-nothing.html](do-nothing.html).  This is an ordinary Web page including no Go code whatsoever.  It demonstrates that Go code is not required to appear on a Web page handled by the Go Server Pages Apache module and can also be used to test basic Web server functionality once Go Server Pages is installed.

* [trivial-expr.html](trivial-expr.html).  This page shows how to embed a simple arithmetic expression within a Web page by writing it as `<?go:expr (1+2*3+4)/5 ?>`.

* [trivial-block.html](trivial-block.html).  This page demonstrates two things: expressing a Go **for** loop within `<?go:block` … `?>` markup and using ordinary HTML, as opposed to Go code, as the body of that loop.

* [trivial-top.html](trivial-top.html).  This page first uses `<?go:top` … `?>` markup to import the `fmt` and `io` packages and define a `SayHelloFrom` function.  Later in the page, it invokes `SayHelloFrom` within `<?go:block` … `?>` markup.

* [trivial-error.html](trivial-error.html).  Don't be surprised if accessing this page returns an error message.  That's what it's supposed to do.  It invokes `gosp.SetHTTPStatus` to return an HTTP Too Many Requests (429) error code.  (Too Many Requests was chosen arbitrarily.)

* [plain-text.txt](plain-text.txt).  Go Server Pages do not need to be HTML.  This example demonstrates that a plain-text file can also include server-side Go code.

* [requestdata.html](requestdata.html).  This page outputs a few bits of information passed to it by the Web server.

* [include.html](include.html).  This page invokes `<?go:include include-1.inc ?>` to include the file [include-1.inc](include-1.inc).  include-1.inc in turn invokes `<?go:include includes/include-2.inc ?>` to include the file [includes/include-2.inc](includes/include-2.inc).  The result is as if each included file had been pasted into its including file.

* [redirect.html](redirect.html).  This page shows how to use `gosp.SetHeaderField` and `gosp.SetHTTPStatus` to redirect the client to a different page.

* [post.html](post.html).  This page asks the user for his/her name then reloads the page with the name passed to it via an HTTP `POST` operation.  Given a name, the page outputs it.

* [mime-types.html](mime-types.html).  As a more sophisticated variant of [post.html](post.html), this page asks the user to select an image type from the set {`PNG`, `JPEG`, `GIF`}.  It then generates an image of the selected type, sets the page's [MIME type](https://en.wikipedia.org/wiki/Media_type) accordingly, and sends the image to the client.

* [cookies.html](cookies-types.html).  A "cookie" is a small chunk of data that a Web server can send to a client to store.  This cookie can later be retrieved by the Web server.  This page presents the client with a cookie that tallies the number of times the page has been visited.

* [package.html](package.html).  It is often convenient to define a set of common page operations such as constructing a menu bar or page footer and put these in a helper package that various Go Server Pages can import.  This page assumes that [helper.go](helper.go) appears in its `GOPATH` (specified using the [`GospGoPath` configuration option](https://gosp.pakin.org/configure.html)).  It does an `import "helper"` and twice invokes its `helper.Bottles` function.
