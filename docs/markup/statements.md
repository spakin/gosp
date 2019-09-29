---
title: Statements
parent: Markup
nav_order: 2
---

Statements
==========

Any Go code that can appear within a function body can be used within `?go:block` markup.  This includes variable declarations, loops, conditionals, assignments, and so forth.  All `?go:block` fragments are executed in the same scope so a variable declared in one fragment can be reused in a later fragment.  The following is a simple example of using a Go `for` loop to generate list items:

```html
<ol>
<?go:block
for i := 1; i <= 10; i++ {
	gosp.Fprintf(gospOut, "  <li>%d + %d is %d.</li>\n", i, i, i*2)
?>
</ol>
```
(`gospOut` is a predefined [`io.Writer`](https://golang.org/pkg/io/#Writer) that writes to the Web page itself.)  Go blocks can conveniently be intermixed with Web-page text, which is treated as if it were a `gosp.Fprintf(gospOut, …)` itself:
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

`return` (with no arguments) is allowed.  No further Go markup will be executed after a `return`, and no further HTML will be returned to the client.
