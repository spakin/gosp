##########################################
# Build the Go Server Pages helper tools #
#                                        #
# By Scott Pakin <scott+gosp@pakin.org>  #
##########################################

GOSP_GO_FLAGS = -ldflags="-X main.AutoKillMinutes=$(AUTO_KILL_MINUTES)"
GOPATH := $(abspath .):$(GOPATH)
GOSP2GO_DEPS = \
	src/gosp2go/gosp2go.go \
	src/gosp2go/boilerplate.go \
	src/gosp/gosp.go
GOSP_SERVER_DEPS = \
	src/gosp-server/gosp-server.go \
	src/gosp/gosp.go

all: bin/gosp2go bin/gosp-server

bin/gosp2go: $(GOSP2GO_DEPS)
	$(GO) build $(GOFLAGS) $(GOSP_GO_FLAGS) -o bin/gosp2go gosp2go

bin/gosp-server: $(GOSP_SERVER_DEPS)
	$(GO) build $(GOFLAGS) $(GOSP_GO_FLAGS) -o bin/gosp-server gosp-server

clean:
	$(RM) bin/gosp2go bin/gosp-server

install: bin/gosp2go bin/gosp-server
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) bin/gosp2go $(DESTDIR)$(bindir)
	$(INSTALL) bin/gosp-server $(DESTDIR)$(bindir)

.PHONY: all clean install
