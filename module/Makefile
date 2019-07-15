###############################################
# Build the Apache module for Go Server Pages #
#                                             #
# By Scott Pakin <scott+gosp@pakin.org>       #
###############################################

GENFILES = \
	mod_gosp.la \
	mod_gosp.lo \
	mod_gosp.slo \
	.libs/mod_gosp.lai \
	.libs/mod_gosp.o \
	.libs/mod_gosp.so


all: $(GENFILES)

$(GENFILES): mod_gosp.c
	$(APXS) -c mod_gosp.c

clean:
	$(RM) $(GENFILES)
	$(RM) -r .libs

# WARNING: "make install" does not honor prefix or DESTDIR.  It always
# installs into the current Apache modules directory.
install: mod_gosp.la
	$(APXS) -i -a mod_gosp.la

.PHONY: all clean install
