##########################################
# Build the Go Server Pages helper tools #
#                                        #
# By Scott Pakin <scott+gosp@pakin.org>  #
##########################################

EXT_GOPATH := $(abspath .):$(GOPATH)
GOSP2GO_DEPS = \
	src/gosp2go/gosp2go.go \
	src/gosp2go/boilerplate.go \
	src/gosp2go/params.go \
	src/gosp2go/utils.go \
	src/gosp/gosp.go
GOSP_SERVER_DEPS = \
	src/gosp-server/gosp-server.go \
	src/gosp/gosp.go
VERSION_FLAG = -ldflags="-X main.Version=$(VERSION)"

all: bin/gosp2go bin/gosp-server

bin/gosp2go: $(GOSP2GO_DEPS)
	env GOPATH="$(EXT_GOPATH)" $(GO) build $(GOFLAGS) $(VERSION_FLAG) -o bin/gosp2go gosp2go

bin/gosp-server: $(GOSP_SERVER_DEPS)
	env GOPATH="$(EXT_GOPATH)" $(GO) build $(GOFLAGS) $(VERSION_FLAG) -o bin/gosp-server gosp-server

clean:
	$(RM) bin/gosp2go bin/gosp-server

# Note that we need to rebuild gosp-server as part of "make install" so it uses
# the exact same GOPATH as any plugins we later build with gosp2go.
install: bin/gosp2go bin/gosp-server
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/gosp2go $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 bin/gosp-server $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)/pkg
	$(INSTALL) -m 0755 -d $(DESTDIR)$(gospgodir)/src/gosp
	$(INSTALL) -m 0644 src/gosp/gosp.go $(DESTDIR)$(gospgodir)/src/gosp
	env GOPATH="$(DESTDIR)$(gospgodir):$(GOPATH)" $(GO) install $(GOFLAGS) gosp
	env GOPATH="$(DESTDIR)$(gospgodir):$(GOPATH)" $(GO) build $(GOFLAGS) -o "$(DESTDIR)$(bindir)/gosp-server" src/gosp-server/gosp-server.go
	$(INSTALL) -m 0755 -d $(DESTDIR)$(man1dir)
	cat src/gosp2go/gosp2go.1 | $(AWK) 'NR == 1 {printf ".TH GOSP2GO \"1\" \"%s\" \"v%s\" \"User Commands\"\n", DATE, VERSION} NR > 1' DATE="$$(date +'%B %Y')" VERSION="$(VERSION)" > $(DESTDIR)$(man1dir)/gosp2go.1
	chmod 0644 $(DESTDIR)$(man1dir)/gosp2go.1
	cat src/gosp-server/gosp-server.1 | $(AWK) 'NR == 1 {printf ".TH GOSP-SERVER \"1\" \"%s\" \"v%s\" \"User Commands\"\n", DATE, VERSION} NR > 1' DATE="$$(date +'%B %Y')" VERSION="$(VERSION)" > $(DESTDIR)$(man1dir)/gosp-server.1
	chmod 0644 $(DESTDIR)$(man1dir)/gosp-server.1

.PHONY: all clean install
