Go Server Pages
===============

Description
-----------

Go Server Pages, or "Gosp" for short, is a mechanism for dynamically generating Web pages via server-side code execution.  If you're familiar with [PHP](https://www.php.net/) or [JavaServer Pages](https://en.wikipedia.org/wiki/JavaServer_Pages), Go Server Pages is just like those but based on the [Go programming language](https://golang.org/).  The basic idea is that you can embed Go code right in the middle of an HTML document—or in fact any type of document that your Web server can serve.  The Web server executes the code and includes its output in the page, right where the code originally appeared.  The process is completely invisible to clients, making Go Server Pages 100% compatible with every Web browser in existence.

Because Go Server Pages cause code to execute server-side, much thought was given to security in the project's design:

* Go itself is a strongly typed language with a simple-to-reason-about semantics, which reduces the likelihood of inadvertently introducing security flaws into the code used on a Web page.

* Go Server Pages are [secure by default](https://en.wikipedia.org/wiki/Secure_by_default).  The Go code appearing on a page is allowed to import only those Go packages explicitly authorized by the Web administrator.  Such authorizations can even be granted on a page-by-page basis, thereby supporting the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege) for each individual Go Server Page.

* All data provided by the client (and therefore to be considered untrustworthy and potentially malicious) is quarantined within a single data structure passed to a Go Server Page.  Doing so facilitates distinguishing untrustworthy data from the rest of the program's data.

Example
-------

Here's a trivial example of a Go Server Page:

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

Usage
-----

Go Server Pages is designed for simplicity.  Only the following new markup is introduced:

| Markup                       | Meaning                                     |
| :--------------------------- | :------------------------------------------ |
| &lt;?go:expr *code* ?&gt;    | Evaluate *code* as a Go expression          |
| &lt;?go:block *code* ?&gt;   | Execute statement or statement block *code* |
| &lt;?go:top *code* ?&gt;     | Declare file-level code *code* (`import`, `func`, `const`, etc.) |
| &lt;?go:include *file* ?&gt; | Include local file *file* as if it were pasted in |

Documentation
-------------

Documentation can be found at https://spakin.github.io/gosp/.

Requirements
------------

Not surprisingly, Go Server Pages requires a [Go](https://golang.org/) installation.  It also currently works only with the [Apache Web server](http://httpd.apache.org/).

Author
------

[Scott Pakin](http://www.pakin.org/~scott/), *scott+gosp@pakin.org*
