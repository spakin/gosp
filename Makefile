#########################################
# Build Go Server Pages                 #
#                                       #
# By Scott Pakin <scott+gosp@pakin.org> #
#########################################

# Change the following as desired.
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
APXS = apxs
APXSFLAGS =
GOFLAGS =
INSTALL = install

# GO should be defined as the absolute path of the go command.
GO = $(shell which go)

###########################################################################

SHELL = /bin/sh
SUBDIRS = tools module
export

all:
	for s in $(SUBDIRS) ; do $(MAKE) -C $$s all ; done

clean:
	for s in $(SUBDIRS) ; do $(MAKE) -C $$s clean ; done

# WARNING: "make install" does not honor prefix or DESTDIR when installing the
# Apache module.  It always installs the module into the current Apache modules
# directory.
install: all
	for s in $(SUBDIRS) ; do $(MAKE) -C $$s install ; done

# Due the the awkwardness warned about above, "make install-no-module" installs
# everything except the Apache module.
install-no-module: all
	$(MAKE) -C tools install

.PHONY: all clean install install-no-module
