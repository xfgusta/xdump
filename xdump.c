#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define VERSION "0.1.1"

#define print_error(...) fprintf(stderr, "xdump: " __VA_ARGS__);

long columns_opt = 16;
long skip_opt = 0;
long length_opt = -1;

void xdump(FILE *file, char *filename) {
    unsigned char buffer[columns_opt];
    long bytes_read = 0;
    long offset = skip_opt;
    long group = columns_opt / 2;

    // skip bytes from the beginning of the file
    if(skip_opt > 0) {
        struct stat st;

        errno = 0;
        if(fstat(fileno(file), &st) == -1) {
            print_error("cannot stat file \"%s\": %s", filename,
                        strerror(errno));
            return;
        }

        // this way the offset will not be greater than the file size
        if(S_ISREG(st.st_mode) && skip_opt > st.st_size)
            offset = st.st_size;

        errno = 0;
        if(fseek(file, skip_opt, SEEK_SET) == -1) {
            // if the file isn't seekable
            if(errno == ESPIPE) {
                long pos = 0;
                while(pos != skip_opt && fgetc(file) != EOF)
                    pos++;

                offset = pos;
            }
        }

        if(ferror(file)) {
            print_error("error while seeking \"%s\"\n", filename);
            return;
        }
    }

    while((bytes_read = fread(buffer, 1, columns_opt, file)) > 0) {
        // print offset
        printf("%08lx", offset);

        printf("  ");

        // print hexadecimal area
        for(long i = 0; i < columns_opt; i++) {
            if(i != 0) {
                putchar(' ');

                if(i % group == 0)
                    putchar(' ');
            }

            // stop interpreting the bytes if the -n option was used
            if(length_opt != -1 && offset + i - skip_opt >= length_opt) {
                printf("  ");
                continue;
            }

            if(i < bytes_read) {
                printf("%02x", buffer[i]);
            } else
                printf("  ");
        }

        printf("  ");

        // print characters area
        for(long i = 0; i < columns_opt; i++) {
            if(i == 0)
                putchar('|');

            if(length_opt != -1 && offset + i - skip_opt >= length_opt) {
                putchar('|');
                break;
            }

            if(i < bytes_read) {
                if(isprint(buffer[i]))
                    putchar(buffer[i]);
                else
                    putchar('.');
            }

            if(i + 1 == bytes_read)
                putchar('|');
        }

        putchar('\n');

        offset += bytes_read;

        if(length_opt != -1 && offset >= length_opt) {
            offset = skip_opt + length_opt;
            break;
        }
    }

    printf("%08lx\n", offset);
}

// strtol wrapper
long str_to_long(char *str) {
    long num;
    char *endptr;

    errno = 0;
    num = strtol(str, &endptr, 10);
    if(errno != 0 || str == endptr) {
        print_error("cannot convert \"%s\" to long int\n", str);
        exit(EXIT_FAILURE);
    }

    return num;
}

void display_help() {
    printf(
        "Usage: xdump [-h] [-v] [-s OFFSET] [-n LENGTH] [FILE...]\n\n"
        "Options:\n"
        "  -h         display this help and exit\n"
        "  -v         output version information and exit\n"
        "  -s OFFSET  skip OFFSET bytes from the beginning of the input\n"
        "  -n LENGTH  interpret only LENGTH bytes of input\n\n"
        "Report bugs to <https://github.com/xfgusta/xdump/issues>\n"
    );
    exit(EXIT_SUCCESS);
}

void display_version() {
    printf("xdump version %s\n", VERSION);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int opt;

    while((opt = getopt(argc, argv, "hvs:n:")) != -1) {
        switch(opt) {
            case 'h':
                display_help();
                break;
            case 'v':
                display_version();
                break;
            case 's':
                skip_opt = str_to_long(optarg);
                break;
            case 'n':
                length_opt = str_to_long(optarg);
                break;
            case '?':
                exit(EXIT_FAILURE);
        }
    }

    // skip to the non-option arguments
    argc -= optind;
    argv += optind;

    if(argc > 0) {
        for(int i = 0; i < argc; i++) {
            FILE *file;
            char *filename = argv[i];

            file = fopen(filename, "r");
            if(!file) {
                print_error("cannot open \"%s\": %s\n", filename,
                            strerror(errno));
                continue;
            }

            // print filename only when more than one was given
            if(argc != 1)
                printf("%s:\n", filename);

            xdump(file, filename);
            fclose(file);

            // separate the output of each file
            if(argc != 1 && i+1 < argc)
                putchar('\n');
        }
    } else {
        xdump(stdin, "(standard input)");
    }

    exit(EXIT_SUCCESS);
}
