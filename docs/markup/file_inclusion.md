---
title: File inclusion
parent: Markup
---

File inclusion
==============

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
