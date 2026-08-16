#include <stdio.h>
#include <string.h>
#include "encode.h"
#include "decode.h"
#include "types.h"

static void print_usage(const char *prog)
{
    printf("\nUsage:\n");
    printf("  Encode: %s -e <image.bmp|image.png> <secret.txt> [output]\n", prog);
    printf("  Encode: %s -e <secret.txt>           (auto-detect image)\n", prog);
    printf("  Decode: %s -d <stego.bmp|stego.png> [output.txt]\n", prog);
    printf("  Decode: %s -d [output.txt]            (auto-detect image)\n\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-e") == 0) {
        EncodeInfoV2 enc = {0};
        if (read_and_validate_encode_args_v2(argv, argc, &enc) != e_success) {
            print_usage(argv[0]);
            return 1;
        }
        if (do_encoding_v2(&enc) != e_success) {
            fprintf(stderr, "ERROR: Encoding failed.\n");
            return 1;
        }
    }
    else if (strcmp(argv[1], "-d") == 0) {
        DecodeInfoV2 dec = {0};
        if (read_and_validate_decode_args_v2(argv, argc, &dec) != e_success) {
            print_usage(argv[0]);
            return 1;
        }
        if (do_decoding_v2(&dec) != e_success) {
            fprintf(stderr, "ERROR: Decoding failed.\n");
            return 1;
        }
    }
    else {
        fprintf(stderr, "ERROR: Unknown operation: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
