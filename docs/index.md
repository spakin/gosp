---
title: Go Server Pages
---

Go Server Pages
===============

Go Server Pages, or "Gosp" for short, is a mechanism for dynamically generating Web pages via server-side code execution.  If you're familiar with [PHP](https://www.php.net/) or [JavaServer Pages](https://en.wikipedia.org/wiki/JavaServer_Pages), Go Server Pages is just like those but based on the [Go programming language](https://golang.org/).  The basic idea is that you can embed Go code right in the middle of an HTML document—or in fact any type of document that your Web server can serve.  The Web server executes the code and includes its output in the page, right where the code originally appeared.  The process is completely invisible to clients, making Go Server Pages 100% compatible with every Web browser in existence.

Because Go Server Pages cause code to execute server-side, much thought was given to security in the project's design:

* Go itself is a strongly typed language with a simple-to-reason-about semantics, which reduces the likelihood of inadvertently introducing security flaws into the code used on a Web page.

* Go Server Pages are [secure by default](https://en.wikipedia.org/wiki/Secure_by_default).  The Go code appearing on a page is allowed to import only those Go packages explicitly authorized by the Web administrator.  Such authorizations can even be granted on a page-by-page basis, thereby supporting the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege) for each individual Go Server Page.

* All data provided by the client (and therefore to be considered untrustworthy and potentially malicious) is quarantined within a single data structure passed to a Go Server Page.  Doing so facilitates distinguishing untrustworthy data from the rest of the program's data.

Let's get down to brass tacks.  Here's a trivial example of a Go Server Page:

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <title>My first Go Server Page</title>
  </head>

  <body>
    <p>
      I like the number <?go:expr 2*2*2*3*3*5*7 ?> because it's
      divisible by each integer from 1 to 10.
    </p>
  </body>
</html>
```

The only difference from ordinary HTML in the above is the `<?go:expr 2*2*2*3*3*5*7 ?>` markup.  This tells the Web server to evaluate `2*2*2*3*3*5*7` as a Go expression and replace the tag with the result, producing

    I like the number 2520 because it's…

in the HTML sent back to the user's Web browser.  The user has no way of knowing that `2520` was generated dynamically, not entered statically in the HTML.

[More examples](https://github.com/spakin/gosp/tree/master/examples) are available for your perusal.

Go Server Pages is free and open-source software.  [Download it from GitHub](https://github.com/spakin/gosp) today.

Learn more
----------

* [Installation](install.md)
* [Configuration](configure.md)
* [Including Go code on a Web page](markup.md)
* [Predefined packages, variables, and functions](predefined.md)
* [Overview of the implementation](implementation.md)
