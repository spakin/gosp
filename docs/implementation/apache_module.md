---
title: Apache module
---

Apache module
=============

Go Server Pages integrates with the [Apache Web server](https://httpd.apache.org/) in the form of an Apache module.  The source code for the Apache module lies in the [`module`](https://github.com/spakin/gosp/tree/master/module) directory.  The module performs a number of tasks:

* processing Go Server Pages-related directives appearing in the configuration file,

* compiling pages to Go plugins using `gosp2go`,

* running Go plugins using `gosp-server`,

* sending client request data to the Go plugin, and

* sending the output of the Go plugin back to the client.

The following flowchart illustrates the Go Server Pages module's control flow when processing a request from a client:

![Module control flow](assets/img/gosp-flowchart.svg "Go Server Pages module control flow")

The left side of the flowchart corresponds to the common case of an up-to-date version of the Go Server Pages plugin already being running.  The right side of the flowchart corresponds to the plugin not existing, in which case it is compiled and launched; or outdated, in which case it is stopped, recompiled, and launched.  While not shown in the figure, the actions on the right side of the figure are protected by a mutex to ensure that concurrent accesses to an outdated or nonexistent plugin do not trigger multiple compilations or launches of the same plugin.

If the Abort state in the flowchart is reached, the Go Server Pages Apache module returns to the client an HTTP Internal Server Error (status code 500).
