---
title: gosp2go man page
nav_exclude: true
---

# GOSP2GO

## NAME

<p style="margin-left:11%; margin-top: 1em">gosp2go -
compile a Go Server Page to Go code</p>

## SYNOPSIS

<p style="margin-left:11%; margin-top: 1em"><b>gosp2go</b>
[<i>options</i>] [<i>input_file.gosp</i>] [--
[<i>gosp-server options</i>]]</p>

## DESCRIPTION

<p style="margin-left:11%; margin-top: 1em"><b>gosp2go</b>
is typically invoked by the Go Server Pages Apache module to
compile a Go Server Page to a plugin that can be launched
with <b>gosp-server</b>. However, it can also be invoked
directly, for example to process the output of a CGI
program.</p>

## OPTIONS

<p style="margin-left:11%; margin-top: 1em"><b>-b</b>,
<b>--build</b></p>

<p style="margin-left:17%;">Compile the generated Go code
to a plugin</p>

<p style="margin-left:11%;"><b>-r</b>, <b>--run</b></p>

<p style="margin-left:17%;">Both compile and, using
<b>gosp-server</b>, execute the generated Go code</p>

<p style="margin-left:11%;"><b>-t</b> <i>num</i>,
<b>--max-top</b>=<i>num</i></p>

<p style="margin-left:17%;">Allow at most <i>num</i>
&lt;?go:top ... ?&gt; blocks per file [default: 1]</p>

<p style="margin-left:11%;"><b>-o</b> <i>file</i>,
<b>--output</b>=<i>file</i></p>

<p style="margin-left:17%;">Write output to file
<i>file</i> (&quot;-&quot; = standard output, the default);
the file type depends on whether <b>--build</b>,
<b>--run</b>, or neither is also specified</p>

<p style="margin-left:11%;"><b>-g</b> <i>file</i>,
<b>--go</b>=<i>file</i></p>

<p style="margin-left:17%;">Use <i>file</i> as the Go
compiler [default: go]</p>

<p style="margin-left:11%;"><b>-a</b> <i>list</i>,
<b>--allowed</b>=<i>list</i></p>

<p style="margin-left:17%;">Specify a comma-separated list
of allowed Go imports; if ALL (the default), allow all
imports; if NONE, allow no imports</p>

<p style="margin-left:11%;"><b>--version</b></p>

<p style="margin-left:17%;">Output the <b>gosp2go</b>
version number and exit</p>

<p style="margin-left:11%;"><b>--help</b></p>

<p style="margin-left:17%;">Output <b>gosp2go</b> usage
information and exit</p>

## SEE ALSO

<p style="margin-left:11%; margin-top: 1em"><b><a href="man-gosp-server.html">gosp-server</a></b>(1)</p>

## AUTHOR

<p style="margin-left:11%; margin-top: 1em">Scott Pakin,
<i>scott+gosp@pakin.org</i></p>
