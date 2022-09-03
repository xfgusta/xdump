CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c11 -O2

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

INSTALL ?= install -p -m 0755
INSTALL_MAN ?= install -p -m 0644

xdump: xdump.c
	$(CC) $(CFLAGS) xdump.c $(LDFLAGS) -o xdump

install: xdump
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) xdump $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) xdump.1 $(DESTDIR)$(MANDIR)/man1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/xdump
	rm -f $(DESTDIR)$(MANDIR)/man1/xdump.1

clean:
	rm -f xdump

.PHONY: install uninstall clean
