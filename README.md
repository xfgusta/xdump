# xdump

The xdump utility is a filter which displays the specified file, or standard input if no file is specified, in hexadecimal and ASCII format. It works like `hexdump -C` by default.

See xdump(1) man page for more details.

## Installing

### Arch Linux

Arch Linux users may use the AUR [xdump](https://aur.archlinux.org/packages/xdump)

```
git clone https://aur.archlinux.org/xdump.git
cd xdump
makepkg -si
```

---

Clone the [xdump git repository](https://github.com/xfgusta/xdump) and go to the directory

```
git clone https://github.com/xfgusta/xdump.git
cd xdump
```

Compile the source code running `make`. Then, you can install xdump running `make install` as root.

The program will be in the `/usr/local/bin` directory.

## Copyright

MIT License

Copyright (c) 2021 Gustavo Costa.
