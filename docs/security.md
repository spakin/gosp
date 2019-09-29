---
title: Security
nav_order: 6
---

Security
========

Because Go Server Pages cause code to execute server-side, much thought was given to security in the project's design:

* Go itself is a strongly typed language with a simple-to-reason-about semantics, which reduces the likelihood of inadvertently introducing security flaws into the code used on a Web page.

* Go Server Pages are [secure by default](https://en.wikipedia.org/wiki/Secure_by_default).  The Go code appearing on a page is allowed to import only those Go packages explicitly authorized by the Web administrator.  Such authorizations can even be granted on a page-by-page basis, thereby supporting the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege) for each individual Go Server Page.

* All data provided by the client (and therefore to be considered untrustworthy and potentially malicious) is quarantined within a single data structure passed to a Go Server Page.  Doing so facilitates distinguishing untrustworthy data from the rest of the program's data.
