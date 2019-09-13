##########################################
# Build the Go Server Pages helper tools #
#                                        #
# By Scott Pakin <scott+gosp@pakin.org>  #
##########################################

GOSP_GO_FLAGS = -ldflags="-X main.autoKillTime=$(AUTO_KILL_TIME)"
GOPATH := $(abspath .):$(GOPATH)
GOSP2GO_DEPS = \
	src/gosp2go/gosp2go.go \
	src/gosp2go/boilerplate.go \
	src/gosp/gosp.go

all: bin/gosp2go

bin/gosp2go: $(GOSP2GO_DEPS)
	$(GO) build $(GOFLAGS) $(GOSP_GO_FLAGS) -o bin/gosp2go gosp2go

clean:
	$(RM) bin/gosp2go

install: bin/gosp2go
	$(INSTALL) -m 0755 -d $(DESTDIR)$(bindir)
	$(INSTALL) bin/gosp2go $(DESTDIR)$(bindir)

.PHONY: all clean install
