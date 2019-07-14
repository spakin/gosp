#########################################
# Build the Go Server Pages helper tool #
#                                       #
# By Scott Pakin <scott+gosp@pakin.org> #
#########################################

all: gosp2go

gosp2go: gosp2go.go
	$(GO) build $(GOFLAGS) -o gosp2go gosp2go.go

clean:
	$(RM) gosp2go

install: gosp2go
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) gosp2go $(DESTDIR)$(bindir)

.PHONY: all clean install
