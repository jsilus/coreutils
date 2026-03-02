#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION 0.1
#define _STR(x) #x
#define STR(x) _STR(x)

static const struct option options[] = {
    { "decode", no_argument, NULL, 'd' },
    { "ignore-garbage", no_argument, NULL, 'i' },
    { "wrap", required_argument, NULL, 'w' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 },
};

void help(const char* program_name, FILE* file, int exit_status) {
    fprintf(file, "Usage: %s [OPTION]... [FILE]\n", program_name);
    fputs("Base64 encode or decode FILE, or standard input, to standard output\n", file);
    fputs("  -d, --decode          decode data\n", file);
    fputs("  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n", file);
    fputs("  -w,  --wrap=COLS      wrap encoded lines after COLS characters (default 76)\n\
                        Use 0 to disable line wrapping\n", file);
    fputs("  -h, --help            display this message and exit\n", file);
    fputs("  -v, --version         output version information and exit\n", file);
    exit(exit_status);
}

void version(void) {
    puts(STR(VERSION));
    exit(EXIT_SUCCESS);
}

char dec_to_enc[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};

#define ENC_TO_DEC(BYTE) ((BYTE) >= 'a' ? (BYTE) - 'a' + 26 : (BYTE) >= 'A' ? (BYTE) - 'A' : (BYTE) >= '0' ? (BYTE) - '0' + 52 : (BYTE) == '+' ? 62 : (BYTE) == '/' ? 63 : 0)

#define DECODED_BLOCK_LENGTH 1024 * 3
#define ENCODED_BLOCK_LENGTH DECODED_BLOCK_LENGTH / 3 * 4

#define min(a,b) ((a) < (b) ? (a) : (b))

#define fail(REASON) { \
    fprintf(stderr, "Failed: %s\n", (REASON)); \
    exit(EXIT_FAILURE); \
}

int main(int argc, char* argv[]) {
    int decode = 0;
    int ignore_garbage = 0;
    int wrap = 76;
    int c;
    while ((c = getopt_long(argc, argv, "diw:hv", options, NULL)) != -1) {
        switch (c) {
        case 'd':
            decode++;
            break;
        case 'i':
            ignore_garbage++;
            break;
        case 'w':
            wrap = strtol(optarg, NULL, 10);
            if (wrap == 0 && errno == EINVAL) {
                fprintf(stderr, "failed to parse number of columns\n");
                help(argv[0], stderr, EXIT_FAILURE);
            }
            break;
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
    if (optind + 1 < argc) {
        fprintf(stderr, "%s: extra operand '%s'\n", argv[0], argv[optind]);
        help(argv[0], stderr, EXIT_FAILURE);
    }

    const char *filename;
    if (optind < argc) {
        filename = argv[optind];
    } else {
        filename = "-";
    }

    FILE *file;
    if (strncmp(filename, "-", sizeof "-") == 0) {
        file = stdin;
    } else {
        file = fopen(filename, "rb");
        if (file == NULL)
            fail("File could not be opened");
    }

    if (decode) {
        char buf_in[ENCODED_BLOCK_LENGTH];
        int buf_in_len;
        char buf_out[DECODED_BLOCK_LENGTH];
        int buf_out_len;
        char chunk[4];
        int num_chunks = 0;

        do {
            // fill input buffer
            buf_in_len = 0;
            size_t bytes_read;
            do {
                bytes_read = fread(buf_in + buf_in_len, 1, sizeof buf_in - buf_in_len, file);
                buf_in_len += bytes_read;
            } while (bytes_read > 0 && buf_in_len < sizeof buf_in);
            if (ferror(file))
                fail("Read error");

            // remove all ignored bytes
            if (ignore_garbage) {
                for (int i = 0; i < buf_in_len && buf_in_len > 0;) {
                    char c = buf_in[i];
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') {
                        i++;
                    } else {
                        buf_in_len--;
                        memmove(buf_in + i, buf_in + i + 1, buf_in_len - i);
                    }
                }
            }

            // convert output buffer
            buf_out_len = 0;
            for (int i = 0; i < buf_in_len && buf_in[i] != '='; i++) {
                chunk[num_chunks++] = ENC_TO_DEC(buf_in[i]);
                if (num_chunks == 4) {
                    buf_out[buf_out_len++] = (chunk[0] << 2) | (chunk[1] >> 4);
                    buf_out[buf_out_len++] = (chunk[1] << 4) | (chunk[2] >> 2);
                    buf_out[buf_out_len++] = (chunk[2] << 6) | (chunk[3]);
                    num_chunks = 0;
                    memset(chunk, 0, sizeof chunk);
                }
            }
            if (fwrite(buf_out, 1, buf_out_len, stdout) < buf_out_len)
                fail("Write error");

        } while (!feof(file));

        // handle excess data
        buf_out_len = 0;
        if (num_chunks > 1)
            buf_out[buf_out_len++] = (chunk[0] << 2) | (chunk[1] >> 4);
        if (num_chunks > 2)
            buf_out[buf_out_len++] = (chunk[1] << 4) | (chunk[2] >> 2);
        if (num_chunks > 3)
            buf_out[buf_out_len++] = (chunk[2] << 6) | (chunk[3]);
        if (fwrite(buf_out, 1, buf_out_len, stdout) < buf_out_len)
            fail("Write error");
    } else {
        char buf_in[DECODED_BLOCK_LENGTH];
        int buf_in_len;
        char buf_out[ENCODED_BLOCK_LENGTH];
        int buf_out_len;
        int current_column = 0;

        do {
            // fill input buffer
            buf_in_len = 0;
            size_t bytes_read;
            do {
                bytes_read = fread(buf_in + buf_in_len, 1, sizeof buf_in - buf_in_len, file);
                buf_in_len += bytes_read;
            } while (bytes_read > 0 && buf_in_len < sizeof buf_in);
            if (ferror(file))
                fail("Read error");

            // convert output buffer
            buf_out_len = 0;
            int i;
            for (i = 0; i + 3 < buf_in_len; i += 3) {
                buf_out[buf_out_len++] = dec_to_enc[(buf_in[i] & 0xFC) >> 2];
                buf_out[buf_out_len++] = dec_to_enc[((buf_in[i] & 0x03) << 4) | ((buf_in[i + 1] & 0xF0) >> 4)];
                buf_out[buf_out_len++] = dec_to_enc[((buf_in[i + 1] & 0x0F) << 2) | ((buf_in[i + 2] & 0xC0) >> 6)];
                buf_out[buf_out_len++] = dec_to_enc[buf_in[i + 2] & 0x3F];
            }

            // handle excess data
            if (i < buf_in_len) {
                buf_out[buf_out_len++] = dec_to_enc[(buf_in[i] & 0xFC) >> 2];
                buf_out[buf_out_len++] = dec_to_enc[((buf_in[i] & 0x03) << 4) | ((buf_in[i + 1] & 0xF0) >> 4)];
                if (i + 1 < buf_in_len)
                    buf_out[buf_out_len++] = dec_to_enc[((buf_in[i + 1] & 0x0F) << 2) | ((buf_in[i + 2] & 0xC0) >> 6)];
                else
                    buf_out[buf_out_len++] = '=';
                if (i + 2 < buf_in_len)
                    buf_out[buf_out_len++] = dec_to_enc[buf_in[i + 2] & 0x3F];
                else
                    buf_out[buf_out_len++] = '=';
            }

            // print output wrapped nicely
            if (wrap == 0) {
                if (fwrite(buf_out, 1, buf_out_len, stdout) < buf_out_len)
                    fail("Write error");
            } else {
                int bytes_written = 0;
                while (bytes_written < buf_out_len) {
                    int bytes_to_write = min(wrap - current_column, buf_out_len - bytes_written);
                    if (bytes_to_write == 0) {
                        if (fputc('\n', stdout) == EOF)
                            fail("Write error");
                        current_column = 0;
                    } else {
                        if (fwrite(buf_out + bytes_written, 1, bytes_to_write, stdout) < bytes_to_write)
                            fail("Write error");
                        current_column += bytes_to_write;
                        bytes_written += bytes_to_write;
                    }
                }
            }

        } while (!feof(file) || (wrap != 0 && current_column == 0));
        if (fputc('\n', stdout) == EOF)
            fail("Write error");
    }

    if (fclose(file) == EOF)
        fail("File close error");

    exit(EXIT_SUCCESS);
}
