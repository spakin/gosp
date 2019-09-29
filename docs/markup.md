---
title: Markup
---

Including Go code on a Web page
===============================

Once Go Server Pages is installed and configured, [Go](https://golang.org/) code can be included on a Web page.  This code runs entirely on the server; the client cannot tell that some parts of the Web page are static while others were generated dynamically by executing Go code.

Markup summary
--------------

Go code is embedded on a Web page using a set of specialized markup commands.  These commands are summarized below:


| Markup                       | Meaning                                     |
| :--------------------------- | :------------------------------------------ |
| &lt;?go:expr *code* ?&gt;    | Evaluate *code* as a Go expression          |
| &lt;?go:block *code* ?&gt;   | Execute statement or statement block *code* |
| &lt;?go:top *code* ?&gt;     | Declare file-level code *code* (`import`, `func`, `const`, etc.) |
| &lt;?go:include *file* ?&gt; | Include local file *file* as if it were pasted in |

Expressions
-----------

The simplest Go code that can be included on a Web is at the level of an expression.  Any [Go expression](https://golang.org/ref/spec#Expressions)—including constants, variables, operators, function calls, etc.—can be used.  The only restriction is that the result be convertible to a `string`.  In other words, if the expression can serve as argument to [`fmt.Sprintf("%v", …)`](https://golang.org/pkg/fmt/#Sprintf), it can appear within `?go:expr` markup.

As a very simple example,
```html
<p>One-third is approximately <?go:expr 1.0/3.0 ?>.</p>
```
appears to the client exactly as if the page had been written
```html
<p>One-third is approximately 0.3333333333333333.</p>
```
Expressions can invoke any function that has previously been imported or defined.  For example, if the [`fmt` package](https://golang.org/pkg/fmt/) has been imported,
```html
<p>One-third is written as <?go:expr fmt.Sprintf("%e", 1.0/3.0) ?> in scientific notation.</p>
```
produces
```html
<p>One-third is written as 3.333333e-01 in scientific notation.</p>
```

Statements
----------

Any Go code that can appear within a function body can be used within `?go:block` markup.  This includes variable declarations, loops, conditionals, assignments, and so forth.  All `?go:block` fragments are executed in the same scope so a variable declared in one fragment can be reused in a later fragment.  The following is a simple example of using a Go `for` loop to generate list items:

```html
<ol>
<?go:block
for i := 1; i <= 10; i++ {
	fmt.Fprintf(gospOut, "  <li>%d + %d is %d.</li>\n", i, i, i*2)
?>
</ol>
```
(`gospOut` is a predefined [`io.Writer`](https://golang.org/pkg/io/#Writer) that writes to the Web page itself.)  Go blocks can conveniently be intermixed with Web-page text, which is treated as if it were a `fmt.Fprintf(gospOut, …)` itself:
```html
<ul>
<?go:block
for i := 0; i < 5; i++ {
?>
  <li>This is a line of ordinary HTML, repeated five times.</li>
<?go:block
}
?>
</ul>
```

It is allowed to use `return` (no arguments).  No further Go markup will be executed, and no further HTML will be returned to the client.

Top-level code
--------------

Code intended to appear at the top level of a Go program, right after the `package` declaration, can be specified with `?go:top` markup.  This include function, type, constant, and variable declarations and package imports.  Remember, though, that the only packages that can be imported are those authorized by the Web administrator, as discussed in [Configuring Go Server Pages](configure.md).

Unlike `?go:expr` and `?go:block` markup, `?go:top` blocks are not executed in place.  Instead, they are gathered sequentially into a single chunk of code from wherever they occur on the page:
```html
<?go:top
import (
        "strings"
)
?>
<p>I just wanted to say, <q><?go:expr Admire("Go Server Pages") ?></q>.</p>

<?go:top
func Admire(obj string) string {
	return strings.ToUpper(obj + " is great!")
}
?>
```
Although the second `?go:top block` in the above would more naturally be placed earlier or even merged with the first `?go:top block`, it appears at the bottom of the code snippet to demonstrate that this is a valid construction.

File inclusion
--------------

`?go:include` names a file in the local filesystem to include in place.  The file can be specified with either an absolute path or relative to the page.  `?go:include` is semantically equivalent to the C preprocessor's `#include` directive in that it (a) performs its operation before any code-execution markup (`?go:expr`, `?go:block`, or `?go:top`) and (b) is oblivious to the surrounding language semantics.  In other words, if one Go Server Page includes the text `<block<?go:include other-half.inc ?>`, and if `other-half.inc` begins with the string `quote>`, the text will expand to `<blockquote>`.  Suggestion: Don't write code like that. 😉  A more practical example might be as follows:
```html
<p>Blah, blah, blah.</p>
<?go:include footer.inc ?>
```
where `footer.inc` contains HTML such as
```html
<footer>
  <address>
    You can find us at<br />
    84 Rainey Street<br />
    Arlen, Texas<br />
    USA
  </address>
</footer>
```

For security reasons, `?go:include` can include files that lie in either the same directory or a subdirectory of the Go Server Page.  Hence, a Go Server Page located at `/var/www/showcase/index.html` could invoke `<?go:include helper.inc ?>` to paste in `/var/www/showcase/helper.inc` or `<?go:include includes/fragments/header.inc ?>` to paste in `/var/www/showcase/includes/fragments/header.inc`.  However, in this case, `<?go:include /etc/passwd ?>` would result in an error.

Whitespace removal
------------------

As an aesthetic convenience, whitespace is removed after `<?go:block … ?>` and `<?go:top … ?>` markup.  More precisely, all whitespace up to and including the first newline character is discarded.  Hence,
```html
<!DOCTYPE html>
<html lang="en">
<head>
<title>My Go Server Page</title>
<?go:top
import "fmt"
?>
</head>
```
renders as
```html
<!DOCTYPE html>
<html lang="en">
<head>
<title>My Go Server Page</title>
</head>
```
not as
```html
<!DOCTYPE html>
<html lang="en">
<head>
<title>My Go Server Page</title>

</head>
```
Again, this is merely an aesthetic convenience and should not normally impact the correctness of the output.

No whitespace is discarded after `<?go:expr … ?>`  or `<?go:include … ?>`.
