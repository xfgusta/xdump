ALL=xdump

CC=gcc
CFLAGS=-g -O2 -Wall -Wextra -pedantic -std=c99

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

all: src/$(ALL)

install: all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m0755 src/$(ALL) $(DESTDIR)$(BINDIR)
	install -m0644 man/$(ALL:=.1) $(DESTDIR)$(MANDIR)/man1

clean:
	rm -f src/$(ALL)
