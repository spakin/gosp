#########################################
# Build Go Server Pages                 #
#                                       #
# By Scott Pakin <scott+gosp@pakin.org> #
#########################################

# Change the following as desired.
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
GO = go
GOFLAGS =
INSTALL = install

###########################################################################

SHELL = /bin/sh
SUBDIRS = tools
export

all:
	for s in $(SUBDIRS) ; do cd $$s && $(MAKE) all ; done

clean:
	for s in $(SUBDIRS) ; do cd $$s && $(MAKE) clean ; done

install:
	for s in $(SUBDIRS) ; do cd $$s && $(MAKE) install ; done

.PHONY: all clean install
