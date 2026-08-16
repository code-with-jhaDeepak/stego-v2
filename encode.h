#ifndef ENCODE_H
#define ENCODE_H

#include "types.h"
#include "image_io.h"
#include <stdint.h>

typedef struct
{
    char *src_image_fname;
    char *secret_fname;
    char *stego_image_fname;
    int   auto_detected;      /* 1 if image was auto-detected */
} EncodeInfoV2;

Status read_and_validate_encode_args_v2(char *argv[], int argc, EncodeInfoV2 *enc);
Status do_encoding_v2(EncodeInfoV2 *enc);

#endif
