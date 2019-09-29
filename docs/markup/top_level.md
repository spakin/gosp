---
title: Top-level code
parent: Markup
---

Top-level code
==============

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
