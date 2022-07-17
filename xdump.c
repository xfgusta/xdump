#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define XDUMP_VERSION "0.1.1"

#define isascii(c) (((c) & ~0x7f) == 0)

#define C_RESET      "\033[0m"
#define C_PRINTABLE  "\033[38;5;115m"
#define C_WHITESPACE "\033[38;5;228m"
#define C_ASCII      "\033[38;5;117m"
#define C_NUL        "\033[38;5;248m"

bool color = false;

long column = 16;
long skip_size = 0;
long length = 0;

void xdump(FILE *file);
void file_skip(FILE *file);
long get_adjusted_column();
void str_to_long(char *s, long *n);
int get_spaces(long n);
void usage();

int main(int argc, char **argv) {
    bool adjust_column = false;

    int opt;
    while((opt = getopt(argc, argv, "hvacw:s:n:")) != -1) {
        switch(opt) {
            case 'h':
                usage();
                return 0;
            case 'v':
                puts(XDUMP_VERSION);
                return 0;
            case 'a':
                adjust_column = true;
                break;
            case 'c':
                color = true;
                break;
            case 'w':
                str_to_long(optarg, &column);
                break;
            case 's':
                str_to_long(optarg, &skip_size);
                break;
            case 'n':
                str_to_long(optarg, &length);
                break;
        }
    }

    if(adjust_column)
        column = get_adjusted_column();

    int c = argc - optind;
    if(c > 0) {
        FILE *file = fopen(argv[optind], "rb");
        if(!file) {
            fprintf(stderr, "Cannot open %s: %s\n", argv[optind],
                    strerror(errno));
            exit(1);
        }
        xdump(file);
        fclose(file);
    } else
        xdump(stdin);

    return 0;
}

void xdump(FILE *file) {
    if(skip_size > 0)
        file_skip(file);

    unsigned char buffer[column];
    unsigned char ascii[column+1];

    long addr = skip_size;
    long total = 1;

    for(long bytes = 0; (bytes = fread(buffer, 1, sizeof(buffer), file)) > 0;
            addr += bytes) {
        for(long i = 0; i < bytes; i++, total++) {
            if(!i)
                printf("%08lx  ", addr+i);

            if(color) {
                if(isprint(buffer[i]))
                    printf(C_PRINTABLE "%02x" C_RESET, buffer[i]);
                else if(isspace(buffer[i]))
                    printf(C_WHITESPACE "%02x" C_RESET, buffer[i]);
                else if(!buffer[i])
                    printf(C_NUL "%02x" C_RESET, buffer[i]);
                else if(isascii(buffer[i]))
                    printf(C_ASCII "%02x" C_RESET, buffer[i]);
                else
                    printf("%02x", buffer[i]);
            } else
                printf("%02x", buffer[i]);

            printf("%*c", (i+1 == column/2) ? 2 : 1, ' ');

            ascii[i] = isprint(buffer[i]) ? buffer[i] : '.';

            if(i+1 == bytes || (length && length == total)) {

                ascii[i+1] = 0;

                printf("%*c|", get_spaces(i+1), ' ');

                if(color) {
                    for(long j = 0; j < i+1; j++) {
                        if(isprint(buffer[j]))
                            printf(C_PRINTABLE "%c" C_RESET, ascii[j]);
                        else if(isspace(buffer[j]))
                            printf(C_WHITESPACE "." C_RESET);
                        else if(!buffer[j])
                            printf(C_NUL "." C_RESET);
                        else if(isascii(buffer[j]))
                            printf(C_ASCII "." C_RESET);
                        else
                            putchar(ascii[j]);
                    }
                } else
                    printf("%s", ascii);

                puts("|");

                if(length && length == total) {
                    printf("%08lx  \n", addr+i+1);
                    exit(0);
                }
            }
        }
    }
    printf("%08lx  \n", addr);
}

void file_skip(FILE *file) {
    if(fseek(file, skip_size, SEEK_CUR) == -1) {
        if(errno == ESPIPE) {
            for(long i = 0; i < skip_size; i++) {
                if(fgetc(file) == EOF) {
                    fprintf(stderr, "Reached end of file while skipping\n");
                    exit(1);
                }
            }
        } else {
            fprintf(stderr, "Cannot skip %ld bytes: %s\n", skip_size,
                    strerror(errno));
            exit(1);
        }
    }

    int c = fgetc(file);
    if(c == EOF) {
        fprintf(stderr, "Reached end of file while skipping\n");
        exit(1);
    } else
        ungetc(c, file);
}

long get_adjusted_column() {
    int fd;
    if((fd = openat(AT_FDCWD, "/dev/tty", O_RDWR)) == -1)
        return 16;

    struct winsize winsz;
    if(ioctl(fd, TIOCGWINSZ, &winsz) == -1)
        return 16;

    long col = (winsz.ws_col - 14) / 4;
    return col > 0 ? col : 16;
}

void str_to_long(char *s, long *n) {
    *n = strtol(s, 0, 10);
    if(errno == ERANGE) {
        fprintf(stderr, "Invalid number: %s\n", strerror(errno));
        exit(1);
    } else if(*n <= 0) {
        fprintf(stderr, "Invalid number: Cannot be less than or equal to 0\n");
        exit(1);
    }
}

int get_spaces(long n) {
    int spaces = 0;
    if(n < column)
        spaces = column * 3 - n * 3 + 1;

    if(n < column/2)
        spaces++;

    return spaces;
}

void usage() {
    puts("Usage: xdump [-hv] [-ac] [-w columns] [-s offset] [-n length] "
         "[file]");
}
