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
GO = go
GOFLAGS =
INSTALL = install

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
install:
	for s in $(SUBDIRS) ; do $(MAKE) -C $$s install ; done

.PHONY: all clean install
