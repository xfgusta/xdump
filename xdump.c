#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define VERSION "0.1.1"

#define print_error(...) fprintf(stderr, "xdump: " __VA_ARGS__);

int no_color_is_on;
long skip_opt;
long length_opt = -1;
long columns_opt = 16;
long group_opt = -1;

// XDUMP_COLORS="off=7;bar=7;nul=238;print=7;space=227;ascii=111;nonascii=204"
int color_offset = 7;
int color_bar = 7;
int color_nul = 238;
int color_printable = 7;
int color_whitespace = 227;
int color_ascii = 111;
int color_nonascii = 204;

// get the color code based on the category of the byte
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

// print color output based on the color code if NO_COLOR is off
void pretty_printf(int color_code, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if(!no_color_is_on)
        printf("\033[38;5;%dm", color_code);

    vprintf(fmt, args);

    if(!no_color_is_on)
        printf("\033[0m");

    va_end(args);
}

void xdump(FILE *file, char *filename) {
    unsigned char buffer[columns_opt];
    long bytes_read = 0;
    long offset = skip_opt;
    long group = group_opt > 0 ? group_opt : columns_opt / 2;

    // don't interpret any bytes
    if(length_opt == 0)
        return;

    // skip bytes from the beginning of the file
    if(skip_opt > 0) {
        struct stat st;

        errno = 0;
        if(fstat(fileno(file), &st) == -1) {
            print_error("cannot stat file \"%s\": %s", filename,
                        strerror(errno));
            return;
        }

        // correct the offset if it is greater than the file size
        if(S_ISREG(st.st_mode) && skip_opt > st.st_size)
            offset = st.st_size;

        errno = 0;
        if(fseek(file, skip_opt, SEEK_SET) == -1) {
            // consume byte by byte if the file isn't seekable
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
        pretty_printf(color_offset, "%08lx", offset);

        printf("  ");

        // print hex panel
        for(long i = 0; i < columns_opt; i++) {
            unsigned char c = buffer[i];

            if(i != 0) {
                putchar(' ');

                if(i % group == 0)
                    putchar(' ');
            }

            // fill with spaces when the length is reached
            if(length_opt != -1 && offset + i - skip_opt >= length_opt) {
                printf("  ");
                continue;
            }

            if(i < bytes_read)
                pretty_printf(get_color_code(c), "%02x", c);
            else
                printf("  ");
        }

        printf("  ");

        // print char panel
        pretty_printf(color_bar, "|");

        for(long i = 0; i < columns_opt; i++) {
            unsigned char c = buffer[i];

            // don't print char when the length is reached
            if(length_opt != -1 && offset + i - skip_opt >= length_opt)
                break;

            if(i < bytes_read) {
                if(isprint(c))
                    pretty_printf(color_printable, "%c", c);
                else
                    pretty_printf(get_color_code(c), ".");
            }
        }

        pretty_printf(color_bar, "|");

        // line done
        putchar('\n');

        offset += bytes_read;

        // stop interpreting the bytes when the length is reached and correct
        // the offset if some bytes were skipped
        if(length_opt != -1 && offset - skip_opt >= length_opt) {
            offset = skip_opt + length_opt;
            break;
        }
    }

    pretty_printf(color_offset, "%08lx", offset);
    putchar('\n');
}

// convert a string to a valid 8-bit color code
int str_to_color_code(char *str) {
    int color;
    char *endptr;

    errno = 0;
    color = strtol(str, &endptr, 10);
    if(errno != 0 || str == endptr || (color < 0 || color > 255)) {
        print_error("XDUMP_COLORS: failed to convert the color code for \"%s\""
                    "\n", str);
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
        "Usage: xdump [-h] [-v] [-s OFFSET] [-n LENGTH] [-w COUNT] [-g SIZE] "
        "[FILE...]\n\n"
        "Options:\n"
        "  -h         display this help and exit\n"
        "  -v         output version information and exit\n"
        "  -s OFFSET  skip OFFSET bytes from the beginning of the input\n"
        "  -n LENGTH  interpret only LENGTH bytes of input\n"
        "  -w COUNT   print COUNT hex per line\n"
        "  -g SIZE    group output into SIZE-hex groups\n\n"
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

    while((opt = getopt(argc, argv, "hvs:n:w:g:")) != -1) {
        switch(opt) {
            case 'h':
                display_help();
                break;
            case 'v':
                display_version();
                break;
            case 's':
                skip_opt = str_to_long(optarg);
                if(skip_opt < 0) {
                    print_error("OFFSET cannot be less than zero\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'n':
                length_opt = str_to_long(optarg);
                if(length_opt < 0) {
                    print_error("LENGTH cannot be less than zero\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'w':
                columns_opt = str_to_long(optarg);
                if(columns_opt < 0) {
                    print_error("COUNT cannot be less than zero\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'g':
                group_opt = str_to_long(optarg);
                if(group_opt < 0) {
                    print_error("SIZE cannot be less than zero\n");
                    exit(EXIT_FAILURE);
                }
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
