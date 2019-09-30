---
title: Debugging
nav_order: 7
---

Debugging tips
==============

Recall that a Go Server Page is compiled to a binary plugin then loaded dynamically into a `gosp-server` process.  Consequently, it can be tricky debugging a Go Server Page in vivo.  Perhaps the best way to debug a faulty Go Server Page is to run `gosp-server` from the command line in non-server mode:

```bash
gosp2go --build -o my-page.so myp-page.html
gosp-server --plugin=my-page.so
```

The data passed to the `gosp-server` by the Apache module can be fabricated by storing it in a [JSON](https://json.org/) file and passing that to `gosp-server` with the `--file` option.  For example, a file might contain the following JSON code:
```JSON
{
  "UserData": {
    "Scheme": "http",
    "LocalHostname": "www.example.com",
    "Port": 80,
    "Uri": "/index.html"
  }
}
```
Provide whatever fields from `gosp.RequestData` your particular Go Server Page accesses.

Configuring Apache with [`LogLevel debug`](https://httpd.apache.org/docs/current/mod/core.html#loglevel) will cause Go Server Pages to write a `Handling gosp.RequestData{…}` line to the Apache error-log file (e.g., `error.log`).  This can be helpful for diagnosing requests that lead to incorrect behavior.

Finally, a Go Server Page can invoke [`gosp.LogDebugMessage`](https://godoc.org/github.com/spakin/gosp/tools/src/gosp#LogDebugMessage) to write a debug message to the Apache error-log file, for example with `gosp.LogDebugMessage(gospMeta, "About to do something dangerous")`.  Debug messages appear only if Apache is configured with `LogLevel debug`.
