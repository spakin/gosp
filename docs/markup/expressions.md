---
title: Expressions
parent: Markup
nav_order: 1
---

Expressions
===========

The simplest Go code that can be included on a Web is at the level of an expression.  Any [Go expression](https://golang.org/ref/spec#Expressions)—including constants, variables, operators, function calls, etc.—can be used.  The only restriction is that the result be convertible to a `string`.  In other words, if the expression can serve as the argument to [`fmt.Sprintf("%v", …)`](https://golang.org/pkg/fmt/#Sprintf), it can appear within `?go:expr` markup.

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
