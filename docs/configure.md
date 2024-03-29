---
title: Configuration
nav_order: 3
---

Configuring Go Server Pages
===========================

You will first need to identify the appropriate Apache configuration file for your system, typically a `.conf` file under `/etc/apache2/conf-enabled/`.  Go Server Pages can be configured both globally for all requests and for requests to specific URLs or locations in the filesystem.  See [Configuration Sections](http://httpd.apache.org/docs/current/sections.html) in the Apache documentation for information on how to control the scope of a server directive.

Processing `.html` files as Go Server Pages
-------------------------------------------

The most important configuration step is to tell Apache what pages should be handled by Go Server Pages.  To enable Go code in *all* HTML pages on your server, specify a handler as follows:
```ApacheConf
AddHandler gosp .html
```
To limit Go code to HTML appearing within a specific URI, say `/fancy`, put the `AddHandler` within a `<Location>` directive:
```ApacheConf
<Location "/fancy/">
  AddHandler gosp .html
</Location>
```
Limiting Go usage to HTML files within a particular local directory is similar:
```ApacheConf
<Directory "/var/www/fancy/">
  AddHandler gosp .html
</Directory>
```

Go code does not necessarily need to be embedded in HTML.  It is perfectly reasonable to associate Go Server Pages with other file types, as in
```ApacheConf
AddHandler gosp .txt
```

Supported directives
--------------------

The Go Server Pages Apache module processes the following directives:

| Directive            | Default                                     | Meaning                                                                             |
| :----------------    | :--------------------------------------     | :---------------------------------------------------------------------------------- |
| `GospWorkDir`        | `/var/cache/apache2/mod_gosp`               | Name of a directory in which Gosp can generate files needed during execution        |
| `GospAllowedImports` | `NONE`                                      | Comma-separated list of packages that are allowed to be imported or `ALL` or `NONE` |
| `GospModReplace`     | *none*                                      | Module replacement to write to generated `go.mod` files                             |
| `GospGoPath`         | *some path*`/lib/gosp/go`                   | Value of the `GOPATH` environment variable to use when building a page              |
| `GospGoModCache`     | `$GOPATH/pkg/mod`                           | Value of the `GOMODCACHE` environment variable to use when building a page          |
| `GospMaxIdleTime`    | `0m`                                        | Maximum idle time before a Gosp server automatically exits (0m = infinite)          |
| `GospServer`         | *some_path*`/bin/gosp-server`               | `gosp-server` executable                                                            |
| `GospGoCompiler`     | *some_path*`/bin/go`                        | Go compiler executable                                                              |
| `GospMaxTop`         | `1000000000`                                | Maximum number of `?go:top` blocks allowed per page                                 |

The module also honors the [`User`](https://httpd.apache.org/docs/current/mod/mod_unixd.html#user) and [`Group`](https://httpd.apache.org/docs/current/mod/mod_unixd.html#group) directives defined by the [`mod_unixd`](https://httpd.apache.org/docs/current/mod/mod_unixd.html) module.

**`GospWorkDir`** is needed while the Web server is running.  It contains a `pages` subdirectory that shadows the filesystem structure and contains one shared object per requested URL.  It contains a `sockets` subdirectory that also shadows the filesystem structure and contains one local-domain socket per requested URL.  And it contains a `go-build` directory that caches results of `go build` commands.  It is safe to delete the contents of `GospWorkDir` when the Web server is not running.

**`GospAllowedImports`** specifies the set of [Go packages](https://golang.org/pkg/) that a Go Server Page is allowed to `import`.  This is one of the main security mechanisms Go Server Pages provides.  Ideally, each page should be granted access only to the minimum set of packages it requires to run.  Although `ALL` is supported, note that this means a page can `import "os"` to gain full read/write access to local files or `import "net"` to perform its own network communication.

As a special case, if the argument to `GospAllowedImports` begins with a `+`, the set of allowed packages is appended to its parent's set.  For example, with the configuration
```ApacheConf
GospAllowedImports time,fmt,html,strings

<Directory "/home/trusted/">
    GospAllowedImports +os
</Directory>
```
most pages can import only the `time`, `fmt`, `html`, and `strings` packages.  Pages served from beneath the `/home/trusted` directory, however, can additionally import the `os` package.  (Without the `+`, pages beneath `/home/trusted` would be able to import *only* the `os` package.)

**`GospModReplace`** specifies a [module replacement](https://golang.org/ref/mod#go-mod-file-replace) to be written to the `go.mod` file associated with each Go Server Page.  It takes two arguments: a module name and a replacement module or directory.  The latter can be omitted to cancel a replacement introduced by a parent configuration.  `GospModReplace` can be used multiple times in a configuration.  As an example,
```ApacheConf
GospModReplace serverLocal /var/www/go/src/serverLocal
```
will write `replace serverLocal => /var/www/go/src/serverLocal` into each page's `go.mod` file when compiling the page.  Assuming that `serverLocal` is allowed by `GospAllowedImports`, pages will be able to import and use the `serverLocal` package.  If needed, version numbers can be appended to the module name and/or replacement with `@` as in `serverLocal@v1.2.3`.

**`GospGoPath`** sets the `GOPATH` variable as specified to the specified value during page compilation.  This variable names a directory in which downloaded Go packages can be stored.  (Technically, it can be a `:`-separated list of directories, with an initial `+` indicating that these should be prepended to the parent configuration's `GospGoPath`.  Multi-directory `GOPATH`s have ceased being useful starting with Go 1.16.)

**`GospGoModCache`** sets the `GOMODCACHE` variable to the specified value during page compilation.  This variable names a directory in which module-versioning information can be stored.  Because `GOMODCACHE` defaults to the `pkg/mod` subdirectory of `GOPATH`, `GospModCache` probably never needs to be changed.

**`GospMaxIdleTime`** provides an automatic cleanup mechanism.  To avoid leaving one Go Server Page process running indefinitely per Web page, these processes can exit automatically after `GospMaxIdleTime` of no usage.  The only downside is the (reasonably low) cost of a process launch the next time the page is accessed after a long period of no accesses.  Times are specified as a number followed by a suffix of `s` for seconds, `m` for minutes, or `h` for hours.  `GospMaxIdleTime` should not be set too small or a process could self-terminate before sending back the page's contents.  A few minutes (say, `5m`) is a good value for `GospMaxIdleTime`.

**`GospServer`** points to the `gosp-server` executable.  It should automatically be set correctly.  However, you might consider replacing it with a script that imposes memory or CPU usage limits
(e.g., with [the Bash shell's `ulimit` command](https://linux.die.net/man/1/bash) or the [LimitCPU](http://limitcpu.sourceforge.net/) tool) then launches the real `gosp-server`.

**`GospGoCompiler`** specifies the full path to the Go compiler.  It should automatically be set correctly and probably never needs to be changed.

**`GospMaxTop`** limits the number of top-level blocks of Go code (function/method declarations, `import` blocks, etc.) allowed per page.  The thinking is that if an attacker somehow managed to inject code on a page, this would limit the harm that could be caused.  This is probably of limited use for pages served directly by the Web server, as opposed to pages generated manually via the `gosp2go` command-line tool.  This is why `GospMaxTop` defaults to such a large number.
