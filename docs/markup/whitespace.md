---
title: Whitespace removal
parent: Markup
nav_order: 5
---

Whitespace removal
==================

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
