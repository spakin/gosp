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
export

all:
	$(MAKE) -C tools
	$(MAKE) -C module

clean:
	$(MAKE) -C tools clean
	$(MAKE) -C module clean

# WARNING: "make install" does not honor prefix or DESTDIR when installing the
# Apache module.  It always installs the module into the current Apache modules
# directory.
install: all
	$(MAKE) -C tools install
	$(MAKE) -C module install

# Due the the awkwardness warned about above, "make install-no-module" installs
# everything except the Apache module.
install-no-module: all
	$(MAKE) -C tools install

.PHONY: all clean install install-no-module
