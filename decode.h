#ifndef DECODE_H
#define DECODE_H

#include "types.h"
#include "image_io.h"

typedef struct
{
    char *stego_image_fname;
    char *secret_fname;
    int   auto_detected;
} DecodeInfoV2;

Status read_and_validate_decode_args_v2(char *argv[], int argc, DecodeInfoV2 *dec);
Status do_decoding_v2(DecodeInfoV2 *dec);

#endif
