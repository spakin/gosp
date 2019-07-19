###############################################
# Build the Apache module for Go Server Pages #
#                                             #
# By Scott Pakin <scott+gosp@pakin.org>       #
###############################################

GENFILES = \
	launch.lo \
	launch.o \
	launch.slo \
	mod_gosp.la \
	mod_gosp.la \
	mod_gosp.lai \
	mod_gosp.lo \
	mod_gosp.o \
	mod_gosp.slo \
	mod_gosp.so \
	utils.lo \
	utils.o \
	utils.slo

all: $(GENFILES)

$(GENFILES): mod_gosp.c utils.c launch.c gosp.h
	$(APXS) $(APXSFLAGS) -I. -c mod_gosp.c utils.c launch.c

clean:
	$(RM) $(GENFILES)
	$(RM) -r .libs

# WARNING: "make install" does not honor prefix or DESTDIR.  It always
# installs into the current Apache modules directory.
install: mod_gosp.la
	$(APXS) -i -a mod_gosp.la

.PHONY: all clean install
