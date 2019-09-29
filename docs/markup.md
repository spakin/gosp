---
title: Markup
has_children: true
has_toc: false
---

Including Go code on a Web page
===============================

Once Go Server Pages is installed and configured, [Go](https://golang.org/) code can be included on a Web page.  This code runs entirely on the server; the client cannot tell that some parts of the Web page are static while others were generated dynamically by executing Go code.

Go code is embedded on a Web page using a set of specialized markup commands.  These commands are summarized below:

| Markup                       | Meaning                                     |
| :--------------------------- | :------------------------------------------ |
| &lt;?go:expr *code* ?&gt;    | Evaluate *code* as a Go expression          |
| &lt;?go:block *code* ?&gt;   | Execute statement or statement block *code* |
| &lt;?go:top *code* ?&gt;     | Declare file-level code *code* (`import`, `func`, `const`, etc.) |
| &lt;?go:include *file* ?&gt; | Include local file *file* as if it were pasted in |

For details, see the following pages:

* [Expressions](markup/expressions.md) (`?go:expr … ?>`)
* [Statements](markup/statements.md) (`?go:block … ?>`)
* [Top-level code](markup/top_level.md) (`?go:top … ?>`)
* [File inclusion](markup/file_inclusion.md) (`?go:include … ?>`)
* [Whitespace removal](markup/whitespace.md)
