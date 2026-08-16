#include "decode.h"
#include "image_io.h"
#include "common.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ─── Internal helpers ─────────────────────────────────────────────────── */

/*
 * Decode 'size' bytes from pixel buffer starting at *offset.
 * Each byte costs 8 pixel bytes (one bit per pixel byte).
 */
static Status decode_bytes(const ImageBuffer *buf, uint *offset,
                            uint8_t *out, uint size)
{
    uint capacity = get_image_capacity(buf);
    if (*offset + size * 8 > capacity) {
        fprintf(stderr, "ERROR: Trying to read beyond image capacity\n");
        return e_failure;
    }

    for (uint i = 0; i < size; i++) {
        uint8_t byte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            byte |= (buf->pixels[*offset] & 1) << bit;
            (*offset)++;
        }
        out[i] = byte;
    }
    return e_success;
}

/*
 * Decode a 32-bit integer stored in big-endian order.
 */
static Status decode_uint32(const ImageBuffer *buf, uint *offset, uint32_t *value)
{
    uint8_t bytes[4];
    if (decode_bytes(buf, offset, bytes, 4) != e_success)
        return e_failure;
    *value = ((uint32_t)bytes[0] << 24) |
             ((uint32_t)bytes[1] << 16) |
             ((uint32_t)bytes[2] <<  8) |
             ((uint32_t)bytes[3]);
    return e_success;
}

/* ─── Argument validation ──────────────────────────────────────────────── */

Status read_and_validate_decode_args_v2(char *argv[], int argc, DecodeInfoV2 *dec)
{
    /*
     * Usage:
     *   ./stego -d <stego_image.bmp|.png> [output.txt]
     *   ./stego -d [output.txt]   (auto-detect image)
     */
    if (argc == 2) {
        /* Auto-detect mode: no args after -d */
        dec->stego_image_fname = auto_detect_image();
        if (!dec->stego_image_fname) {
            fprintf(stderr, "ERROR: No BMP or PNG image found in current directory\n");
            return e_failure;
        }
        printf("INFO: Auto-detected image: %s\n", dec->stego_image_fname);
        dec->auto_detected  = 1;
        dec->secret_fname   = "decrypted.txt";
        printf("INFO: Output file not specified. Using: decrypted.txt\n");
    } else if (argc >= 3) {
        /* Check if argv[2] is an image or a txt output */
        ImageFormat fmt = detect_format(argv[2]);
        if (fmt == FORMAT_BMP || fmt == FORMAT_PNG) {
            dec->stego_image_fname = argv[2];
            dec->auto_detected     = 0;
            dec->secret_fname      = (argc >= 4 && argv[3]) ? argv[3] : "decrypted.txt";
            if (argc < 4)
                printf("INFO: Output file not specified. Using: decrypted.txt\n");
        } else {
            /* argv[2] is output txt, auto-detect image */
            dec->secret_fname      = argv[2];
            dec->stego_image_fname = auto_detect_image();
            dec->auto_detected     = 1;
            if (!dec->stego_image_fname) {
                fprintf(stderr, "ERROR: No BMP or PNG image found in current directory\n");
                return e_failure;
            }
            printf("INFO: Auto-detected image: %s\n", dec->stego_image_fname);
        }
    } else {
        fprintf(stderr, "Usage: %s -d <stego_image> [output.txt]\n", argv[0]);
        fprintf(stderr, "       %s -d [output.txt]  (auto-detect image)\n", argv[0]);
        return e_failure;
    }

    return e_success;
}

/* ─── Main decode function ─────────────────────────────────────────────── */

Status do_decoding_v2(DecodeInfoV2 *dec)
{
    printf("INFO: Loading stego image: %s\n", dec->stego_image_fname);
    sleep(1);

    ImageBuffer buf;
    if (load_image(dec->stego_image_fname, &buf) != e_success)
        return e_failure;

    printf("INFO: Image loaded — %u x %u\n", buf.width, buf.height);
    sleep(1);

    uint offset = 0;

    /* ── Verify magic string ── */
    printf("INFO: Verifying magic string...\n");
    sleep(1);
    uint8_t magic[3] = {0};
    if (decode_bytes(&buf, &offset, magic, 2) != e_success) goto fail;
    magic[2] = '\0';

    if (strcmp((char *)magic, MAGIC_STRING) != 0) {
        fprintf(stderr, "ERROR: Magic string not found. "
                        "Image does not contain encoded data.\n");
        free_image(&buf);
        return e_failure;
    }
    printf("INFO: Magic string verified.\n");
    sleep(1);

    /* ── Decode extension size and data ── */
    printf("INFO: Decoding extension info...\n");
    sleep(1);
    uint32_t ext_len = 0;
    if (decode_uint32(&buf, &offset, &ext_len) != e_success) goto fail;
    if (ext_len > 16) {
        fprintf(stderr, "ERROR: Invalid extension length: %u\n", ext_len);
        goto fail;
    }
    uint8_t ext[17] = {0};
    if (decode_bytes(&buf, &offset, ext, ext_len) != e_success) goto fail;
    printf("INFO: Secret file extension: %s\n", ext);
    sleep(1);

    /* ── Decode secret file size ── */
    printf("INFO: Decoding secret file size...\n");
    sleep(1);
    uint32_t secret_size = 0;
    if (decode_uint32(&buf, &offset, &secret_size) != e_success) goto fail;

    if (secret_size == 0 || secret_size > get_image_capacity(&buf) / 8) {
        fprintf(stderr, "ERROR: Invalid secret size decoded: %u\n", secret_size);
        goto fail;
    }
    printf("INFO: Secret data size: %u bytes\n", secret_size);
    sleep(1);

    /* ── Decode secret data ── */
    printf("INFO: Decoding secret data...\n");
    sleep(1);
    uint8_t *secret_data = (uint8_t *)malloc(secret_size + 1);
    if (!secret_data) goto fail;

    if (decode_bytes(&buf, &offset, secret_data, secret_size) != e_success) {
        free(secret_data);
        goto fail;
    }
    secret_data[secret_size] = '\0';

    /* ── Write to output file ── */
    FILE *fptr_out = fopen(dec->secret_fname, "wb");
    if (!fptr_out) {
        fprintf(stderr, "ERROR: Cannot open output file: %s\n", dec->secret_fname);
        free(secret_data);
        goto fail;
    }
    fwrite(secret_data, secret_size, 1, fptr_out);
    fclose(fptr_out);
    free(secret_data);

    free_image(&buf);
    printf("INFO: Decoding Done Successfully.\n");
    printf("INFO: Secret written to: %s\n", dec->secret_fname);
    return e_success;

fail:
    free_image(&buf);
    return e_failure;
}
