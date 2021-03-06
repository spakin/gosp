#########################################
# Build Go Server Pages                 #
#                                       #
# By Scott Pakin <scott+gosp@pakin.org> #
#########################################

# Change the following as desired.
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
man1dir = $(mandir)/man1
docdir = $(datarootdir)/doc/gosp
libdir = $(exec_prefix)/lib
gospdir = $(libdir)/gosp
gospgodir = $(gospdir)/go
AWK = awk
TAR = tar
INSTALL = install
APXS = apxs
APXSFLAGS =
GOFLAGS =

# GO should be defined as the absolute path of the go command.
GO = $(shell which go)

###########################################################################

VERSION = 1.0.0
SHELL = /bin/sh
export

all: bin/gosp2go bin/gosp-server src/module/mod_gosp.la

###########################################################################

# --------------------------------------------------------- #
# Build the Apache module from the src/module subdirectory. #
# --------------------------------------------------------- #

MODULE_GENFILES = \
	.libs/comm.o \
	.libs/launch.o \
	.libs/mod_gosp.la \
	.libs/mod_gosp.lai \
	.libs/mod_gosp.o \
	.libs/mod_gosp.so \
	.libs/utils.o \
	comm.lo \
	comm.slo \
	launch.lo \
	launch.slo \
	mod_gosp.la \
	mod_gosp.lo \
	mod_gosp.slo \
	utils.lo \
	utils.slo

MODULE_C_SOURCES = \
	mod_gosp.c \
	utils.c \
	launch.c \
	comm.c

src/module/mod_gosp.la: $(addprefix src/module/,$(MODULE_C_SOURCES) gosp.h)
	$(APXS) $(APXSFLAGS) \
	  -DDEFAULT_GO_COMMAND='\"$(GO)\"' \
	  -DGOSP2GO='\"$(bindir)/gosp2go\"' \
	  -DGOSP_SERVER='\"$(bindir)/gosp-server\"' \
	  -DDEFAULT_GO_PATH='\"$(gospgodir)\"' \
	  -I. -c $(addprefix src/module/,$(MODULE_C_SOURCES))

###########################################################################

# --------------------------------------- #
# Build the helper tools from the gosp2go #
# and gosp-server subdirectories          #
# --------------------------------------- #

EXT_GOPATH := $(abspath .):$(GOPATH)
GOSP2GO_DEPS = \
	gosp2go/gosp2go.go \
	gosp2go/boilerplate.go \
	gosp2go/params.go \
	gosp2go/utils.go \
	gosp/gosp.go
GOSP_SERVER_DEPS = \
	gosp-server/gosp-server.go \
	gosp-server/params.go \
	gosp-server/write-meta.go \
	gosp-server/serve.go \
	gosp/gosp.go
VERSION_FLAG = -ldflags="-X main.Version=$(VERSION)"

bin/gosp2go: $(addprefix src/,$(GOSP2GO_DEPS))
	env GOPATH="$(EXT_GOPATH)" $(GO) build $(GOFLAGS) $(VERSION_FLAG) -o bin/gosp2go gosp2go

bin/gosp-server: $(addprefix src/,$(GOSP_SERVER_DEPS))
	env GOPATH="$(EXT_GOPATH)" $(GO) build $(GOFLAGS) $(VERSION_FLAG) -o bin/gosp-server gosp-server

###########################################################################

# ----------------------- #
# Install Go Server Pages #
# ----------------------- #

# install-no-module installs everything except the Apache module.

# Note that we need to rebuild gosp-server as part of "make install" so it uses
# the exact same GOPATH as any plugins we later build with gosp2go.
install-no-module: bin/gosp2go bin/gosp-server install-man install-doc
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/gosp2go $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)/pkg
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)/src/gosp
	$(INSTALL) -m 0644 src/gosp/gosp.go $(DESTDIR)$(gospgodir)/src/gosp
	env GOPATH="$(DESTDIR)$(gospgodir):$(GOPATH)" $(GO) install $(GOFLAGS) gosp
	env GOPATH="$(DESTDIR)$(gospgodir):$(EXT_GOPATH)" $(GO) build $(GOFLAGS) -o "$(DESTDIR)$(bindir)/gosp-server" gosp-server

# Install the man pages for gosp2go and gosp-server.
install-man: src/gosp2go/gosp2go.1 src/gosp-server/gosp-server.1
	$(INSTALL) -m 0755 -d $(DESTDIR)$(man1dir)
	cat src/gosp2go/gosp2go.1 | $(AWK) 'NR == 1 {printf ".TH GOSP2GO \"1\" \"%s\" \"v%s\" \"User Commands\"\n", DATE, VERSION} NR > 1' DATE="$$(date +'%B %Y')" VERSION="$(VERSION)" > $(DESTDIR)$(man1dir)/gosp2go.1
	chmod 0644 $(DESTDIR)$(man1dir)/gosp2go.1
	cat src/gosp-server/gosp-server.1 | $(AWK) 'NR == 1 {printf ".TH GOSP-SERVER \"1\" \"%s\" \"v%s\" \"User Commands\"\n", DATE, VERSION} NR > 1' DATE="$$(date +'%B %Y')" VERSION="$(VERSION)" > $(DESTDIR)$(man1dir)/gosp-server.1
	chmod 0644 $(DESTDIR)$(man1dir)/gosp-server.1

install-doc:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(docdir)/examples
	cp -ra examples/* $(DESTDIR)$(docdir)/examples
	$(INSTALL) -m 0644 README.md LICENSE.md $(DESTDIR)$(docdir)

# WARNING: "make install" does not honor prefix or DESTDIR.  It always
# installs into the current Apache modules directory.
install: install-no-module src/module/mod_gosp.la
	$(APXS) -i -a src/module/mod_gosp.la

###########################################################################

# -------------------------------- #
# Create a Go Server Pages tarball #
# -------------------------------- #

TARBASE = gosp-$(VERSION)
DISTFILES = \
	README.md \
	LICENSE.md \
	Makefile \
	docs \
	examples \
	src/gosp \
	src/gosp2go \
	src/gosp-server \
	$(addprefix src/module/,$(MODULE_C_SOURCES) gosp.h)

dist:
	$(RM) -r $(TARBASE) $(TARBASE).tar.gz
	for f in $(DISTFILES) ; do \
	  $(INSTALL) -d -m 0755 $$(dirname $(TARBASE)/$$f) ; \
	  cp -ra $$f $(TARBASE)/$$f ; \
	done
	$(TAR) -czf $(TARBASE).tar.gz $(TARBASE)
	$(RM) -r $(TARBASE)

###########################################################################

# ----------------------------------------- #
# Delete any file we know how to regenerate #
# ----------------------------------------- #

clean:
	$(RM) -r bin
	$(RM) $(addprefix src/module/,$(MODULE_GENFILES))
	$(RM) -r src/module/.libs

.PHONY: all install-no-module install-man install-doc install dist clean
