---
title: Installation
nav_order: 2
---

Installing Go Server Pages
==========================

Go Server Pages is not too difficult to build and install.  The following explains the process.

Prerequisites
-------------

To date, Go Server Pages has been tested only on Linux, specifically [Ubuntu](https://ubuntu.com/).  I'd think it *should* work on macOS, but I'd guess it probably won't work on Windows.  Please [let me know](mailto:scott+gosp@pakin.org) your experience with installing Go Server Pages on a platform other than Linux.  Here's what you'll need:

1. **Apache**.  Go Server Pages is written as an Apache module so we assume you already have the [Apache Web server](https://httpd.apache.org/) (version 2.4+) installed and configured.  Compiling the Go Server Pages module requires [`apxs`, the APache eXtenSion tool](https://httpd.apache.org/docs/current/programs/apxs.html).  In Ubuntu, this can be installed with
```bash
sudo apt install apache2-dev
```

2. **Go**.  [Download a Go compiler](https://golang.org/dl/) to compile the various components of Go Server Pages that are written in [Go](https://golang.org/).  The most recent version of Go Server Pages as of this writing (July 2021) was tested with Go 1.15.5 and 1.16.6, but may work with Go versions as far back as 1.13.  We assume the availability of a [`go` command](https://golang.org/cmd/go/) for invoking the compiler and associated tools.

3. A C compiler (needed to build the Apache module) and [GNU Make](https://www.gnu.org/software/make/).  In Ubuntu, that means
```bash
sudo apt install gcc make
```

Compiling
---------

Go Server Pages builds with a plain-vanilla `Makefile`.  The first stanza of the [Go Server Pages `Makefile`](https://github.com/spakin/gosp/blob/master/Makefile) lists various installation directories, commands, and flags that can be overridden during the build process.

Decide where you want to install Go Server Pages.  The default prefix is `/usr/local`.  By default, the build system installs helper programs in a `bin` subdirectory of the prefix directory, Go packages in a `lib/gosp/go` subdirectory, manual pages in a `share/man` subdirectory, and other documentation in a `share/doc/gosp` subdirectory.  For example, the source code for the `gosp` package will be installed by default as `/usr/local/lib/gosp/go/src/gosp/gosp.go`.  Run
```bash
make vars
```
for a report of installation locations.  If those directories are acceptable, simply run
```bash
make
```
If not, override `prefix` (and/or any of the other variables defined in the `Makefile`) on the command line, e.g.,
```bash
make prefix=/opt/gosp
```
(You can also edit the `Makefile`, but I personally prefer leaving `Makefile`s pristine.)

Installing
----------

**Caveat #1**: The Go Server Pages Apache module will install into whatever directory Apache modules normally get installed into, regardless of the `prefix` and other directory variables.  On an Ubuntu system, this is likely to be `/usr/lib/apache2/modules`.  (Run `make vars` or `a2query -d` to check.)

**Caveat #2**: When installing, you must specify the same `prefix` and other build settings as when you compiled.  This is because the Apache module needs to know where it will be able to find its helper programs and Go packages.

Install using the `install` target:
```bash
make install
```
(or similarly,
```bash
make prefix=/opt/gosp install
```
or the like).  This installs and enables the Go Server Pages Apache module and helper programs/package.  If you would prefer to install everything except the Apache module, use the `install-no-module` target instead of `install`.

You're not done yet.  Once Go Server Pages is installed and enabled the Web server needs to be configured to actually *use* it.  See [Configuring Go Server Pages](configure.md) for details.
