#########################################
# Build the Go Server Pages helper tool #
#                                       #
# By Scott Pakin <scott+gosp@pakin.org> #
#########################################

SOURCES = \
	gosp2go.go \
	boilerplate.go

all: gosp2go

gosp2go: $(SOURCES)
	$(GO) build $(GOFLAGS) -ldflags="-X main.autoKillTime=$(AUTO_KILL_TIME)" -o gosp2go $(SOURCES)

clean:
	$(RM) gosp2go

install: gosp2go
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) gosp2go $(DESTDIR)$(bindir)

.PHONY: all clean install
