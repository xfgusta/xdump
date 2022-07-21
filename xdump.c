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

int no_color_is_on;
long columns_opt = 16;
long skip_opt = 0;
long length_opt = -1;

// XDUMP_COLORS="off=7;bar=7;nul=243;print=83;space=220;ascii=81;nonascii=197"
int color_offset = 7;
int color_bar = 7;
int color_nul = 243;
int color_printable = 83;
int color_whitespace = 220;
int color_ascii = 81;
int color_nonascii = 197;

int get_color_code(unsigned char c) {
    if(c == '\0')
        return color_nul;
    else if(isspace(c))
        return color_whitespace;
    else if(isprint(c))
        return color_printable;
    else if((c & ~0x7f) == 0)
        return color_ascii;
    else
        return color_nonascii;
}

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
        if(no_color_is_on)
            printf("%08lx", offset);
        else
            printf("\033[38;5;%dm%08lx\033[0m", color_offset, offset);

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
                if(no_color_is_on)
                    printf("%02x", buffer[i]);
                else
                    printf("\033[38;5;%dm%02x\033[0m",
                           get_color_code(buffer[i]), buffer[i]);
            } else
                printf("  ");
        }

        printf("  ");

        // print characters area
        for(long i = 0; i < columns_opt; i++) {
            if(i == 0) {
                if(no_color_is_on)
                    putchar('|');
                else
                    printf("\033[38;5;%dm|\033[0m", color_bar);
            }

            if(length_opt != -1 && offset + i - skip_opt >= length_opt) {
                if(no_color_is_on)
                    putchar('|');
                else
                    printf("\033[38;5;%dm|\033[0m", color_bar);
                break;
            }

            if(i < bytes_read) {
                if(isprint(buffer[i])) {
                    if(no_color_is_on)
                        putchar(buffer[i]);
                    else
                        printf("\033[38;5;%dm%c\033[0m", color_printable,
                               buffer[i]);
                } else {
                    if(no_color_is_on)
                        putchar('.');
                    else
                        printf("\033[38;5;%dm.\033[0m",
                               get_color_code(buffer[i]));
                }
            }

            if(i + 1 == bytes_read) {
                if(no_color_is_on)
                    putchar('|');
                else
                    printf("\033[38;5;%dm|\033[0m", color_bar);
            }
        }

        putchar('\n');

        offset += bytes_read;

        if(length_opt != -1 && offset >= length_opt) {
            offset = skip_opt + length_opt;
            break;
        }
    }

    if(no_color_is_on)
        printf("%08lx\n", offset);
    else
        printf("\033[38;5;%dm%08lx\033[0m\n", color_offset, offset);
}

// convert a string to a valid 8-bit color code
int str_to_color_code(char *str) {
    int color;
    char *endptr;

    errno = 0;
    color = strtol(str, &endptr, 10);
    if(errno != 0) {
        print_error("XDUMP_COLORS: strtol failed for \"%s\": %s\n", str,
                    strerror(errno));
        exit(EXIT_FAILURE);
    } else if(str == endptr) {
        print_error("XDUMP_COLORS: no digits were found for \"%s\"\n", str);
        exit(EXIT_FAILURE);
    } else if(color < 0 || color > 255) {
        print_error("XDUMP_COLORS: \"%d\" for \"%s\" is out of range\n", color,
                    str);
        exit(EXIT_FAILURE);
    }

    return color;
}

// parse XDUMP_COLORS format string and set the specified colors
void set_new_colors(char *format) {
    char *token;

    token = strtok(format, ";");
    while(token) {
        char *key = token;
        char *value;

        value = strchr(token, '=');
        if(!value) {
            print_error("XDUMP_COLORS: color code for \"%s\" not found\n", key);
            exit(EXIT_FAILURE);
        }

        // end the key string before the '=' and skip it
        *value++ = '\0';

        if(!strcmp(key, "off"))
            color_offset = str_to_color_code(value);
        else if(!strcmp(key, "bar"))
            color_bar = str_to_color_code(value);
        else if(!strcmp(key, "nul"))
            color_nul = str_to_color_code(value);
        else if(!strcmp(key, "print"))
            color_printable = str_to_color_code(value);
        else if(!strcmp(key, "space"))
            color_whitespace = str_to_color_code(value);
        else if(!strcmp(key, "ascii"))
            color_ascii = str_to_color_code(value);
        else if(!strcmp(key, "nonascii"))
            color_nonascii = str_to_color_code(value);
        else {
            print_error("XDUMP_COLORS: unknown type \"%s\"\n", key);
            exit(EXIT_FAILURE);
        }

        token = strtok(NULL, ";");
    }
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
    char *no_color;
    char *xdump_colors;

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


    // disable colored output when NO_COLOR is present or when the standard
    // output isn't connected to a terminal
    no_color = getenv("NO_COLOR");
    if((no_color && *no_color != '\0') || !isatty(STDOUT_FILENO))
        no_color_is_on = 1;

    // set the colors defined by XDUMP_COLORS
    xdump_colors = getenv("XDUMP_COLORS");
    if(xdump_colors && !no_color_is_on)
        set_new_colors(xdump_colors);

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
            if(argc != 1) {
                if(no_color_is_on)
                    printf("%s:\n", filename);
                else
                    printf("\033[1m%s:\033[0m\n", filename);
            }

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
