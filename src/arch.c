#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#define VERSION 0.1
#define _STR(x) #x
#define STR(x) _STR(x)

static const struct option options[] = {
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 },
};

void help(const char* program_name, FILE* file, int exit_status) {
    fprintf(file, "Usage: %s [OPTION]...\n", program_name);
    fputs("Print machine architecture.\n", file);
    fputs("    --help, -h\n      display this message and exit\n", file);
    fputs("    --version, -v\n      output version information and exit\n", file);
    exit(exit_status);
}

void version(void) {
    puts(STR(VERSION));
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {
    int c;
    while ((c = getopt_long(argc, argv, "hv", options, NULL)) != -1) {
        switch (c) {
        case 'h':
            help(argv[0], stdout, EXIT_SUCCESS);
            break;
        case 'v':
            version();
            break;
        default:
            help(argv[0], stderr, EXIT_FAILURE);
        }
    }
    if (optind < argc) {
        fprintf(stderr, "%s: extra operand '%s'\n", argv[0], argv[optind]);
        help(argv[0], stderr, EXIT_FAILURE);
    }

    struct utsname name;
    int status = uname(&name);
    if (status == -1) {
        fprintf(stderr, "uname error %d: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    puts(name.machine);

    exit(EXIT_SUCCESS);
}
