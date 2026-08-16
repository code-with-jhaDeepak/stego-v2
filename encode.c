#include "encode.h"
#include "image_io.h"
#include "common.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ─── Internal helpers ─────────────────────────────────────────────────── */

/*
 * Encode one bit into the LSB of a pixel byte.
 * bit must be 0 or 1.
 */
static void encode_bit(uint8_t *byte, int bit)
{
    *byte = (*byte & 0xFE) | (bit & 1);
}

/*
 * Encode 'size' bytes from 'data' into the pixel buffer starting at *offset.
 * Each byte of data costs 8 pixel bytes (one bit per pixel byte).
 * Returns e_failure if buffer runs out of space.
 */
static Status encode_bytes(ImageBuffer *buf, uint *offset,
                            const uint8_t *data, uint size)
{
    uint capacity = get_image_capacity(buf);
    if (*offset + size * 8 > capacity) {
        fprintf(stderr, "ERROR: Image too small to hold data\n");
        return e_failure;
    }

    for (uint i = 0; i < size; i++) {
        uint8_t byte = data[i];
        for (int bit = 7; bit >= 0; bit--) {
            encode_bit(&buf->pixels[*offset], (byte >> bit) & 1);
            (*offset)++;
        }
    }
    return e_success;
}

/*
 * Encode a 32-bit integer in big-endian order (fixed, no endianness issues).
 */
static Status encode_uint32(ImageBuffer *buf, uint *offset, uint32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >>  8) & 0xFF;
    bytes[3] = (value      ) & 0xFF;
    return encode_bytes(buf, offset, bytes, 4);
}

/* ─── Argument validation ──────────────────────────────────────────────── */

Status read_and_validate_encode_args_v2(char *argv[], int argc, EncodeInfoV2 *enc)
{
    /*
     * Usage:
     *   ./stego -e <image.bmp|image.png> <secret.txt> [output]
     *   ./stego -e <secret.txt>           (auto-detect image)
     */
    if (argc == 3) {
        /* Auto-detect mode: argv[2] = secret.txt */
        if (strstr(argv[2], ".txt") == NULL) {
            fprintf(stderr, "ERROR: Expected .txt file, got: %s\n", argv[2]);
            return e_failure;
        }
        enc->secret_fname = argv[2];
        enc->src_image_fname = auto_detect_image();
        if (!enc->src_image_fname) {
            fprintf(stderr, "ERROR: No BMP or PNG image found in current directory\n");
            return e_failure;
        }
        printf("INFO: Auto-detected image: %s\n", enc->src_image_fname);
        enc->auto_detected = 1;
    } else if (argc >= 4) {
        /* Explicit mode: argv[2] = image, argv[3] = secret */
        enc->src_image_fname = argv[2];
        enc->secret_fname    = argv[3];
        enc->auto_detected   = 0;

        if (detect_format(enc->src_image_fname) == FORMAT_UNKNOWN) {
            fprintf(stderr, "ERROR: Unsupported image format: %s\n", enc->src_image_fname);
            return e_failure;
        }
        if (strstr(enc->secret_fname, ".txt") == NULL) {
            fprintf(stderr, "ERROR: Secret file must be .txt: %s\n", enc->secret_fname);
            return e_failure;
        }
    } else {
        fprintf(stderr, "Usage: %s -e <image> <secret.txt> [output]\n", argv[0]);
        fprintf(stderr, "       %s -e <secret.txt>  (auto-detect image)\n", argv[0]);
        return e_failure;
    }

    /* Determine output filename */
    if (argc >= 5 && argv[4] != NULL) {
        if (detect_format(argv[4]) == FORMAT_UNKNOWN) {
            fprintf(stderr, "ERROR: Output must be .bmp or .png: %s\n", argv[4]);
            return e_failure;
        }
        /* Output format must match input format */
        if (detect_format(argv[4]) != detect_format(enc->src_image_fname)) {
            fprintf(stderr, "ERROR: Output format must match input format\n");
            return e_failure;
        }
        enc->stego_image_fname = argv[4];
    } else {
        /* Default output name based on input format */
        ImageFormat fmt = detect_format(enc->src_image_fname);
        enc->stego_image_fname = (fmt == FORMAT_PNG) ? "encrypted.png" : "encrypted.bmp";
        printf("INFO: Output file not specified. Using: %s\n", enc->stego_image_fname);
    }

    return e_success;
}

/* ─── Main encode function ─────────────────────────────────────────────── */

Status do_encoding_v2(EncodeInfoV2 *enc)
{
    printf("INFO: Loading source image: %s\n", enc->src_image_fname);
    sleep(1);

    ImageBuffer buf;
    if (load_image(enc->src_image_fname, &buf) != e_success)
        return e_failure;

    printf("INFO: Image loaded — %u x %u, %u channels\n",
           buf.width, buf.height, buf.channels);
    sleep(1);

    /* Open secret file */
    FILE *fptr_secret = fopen(enc->secret_fname, "rb");
    if (!fptr_secret) {
        fprintf(stderr, "ERROR: Cannot open secret file: %s\n", enc->secret_fname);
        free_image(&buf);
        return e_failure;
    }

    /* Get secret file size */
    fseek(fptr_secret, 0, SEEK_END);
    uint32_t secret_size = (uint32_t)ftell(fptr_secret);
    fseek(fptr_secret, 0, SEEK_SET);

    if (secret_size == 0) {
        fprintf(stderr, "ERROR: Secret file is empty\n");
        fclose(fptr_secret);
        free_image(&buf);
        return e_failure;
    }

    /* Get secret file extension */
    const char *ext = strrchr(enc->secret_fname, '.');
    if (!ext) ext = ".txt";
    uint32_t ext_len = (uint32_t)strlen(ext);

    /*
     * Capacity check:
     * magic(2) + ext_size(4) + ext_data(ext_len) + secret_size(4) + secret_data(secret_size)
     * Each byte costs 8 pixel bytes.
     */
    uint capacity = get_image_capacity(&buf);
    uint required = (2 + 4 + ext_len + 4 + secret_size) * 8;
    if (required > capacity) {
        fprintf(stderr, "ERROR: Image capacity %u bytes, required %u bytes\n",
                capacity / 8, required / 8);
        fclose(fptr_secret);
        free_image(&buf);
        return e_failure;
    }

    printf("INFO: Capacity OK — image can hold %u bytes, need %u bytes\n",
           capacity / 8, required / 8);
    sleep(1);

    /* Read secret data */
    uint8_t *secret_data = (uint8_t *)malloc(secret_size);
    if (!secret_data) {
        fclose(fptr_secret);
        free_image(&buf);
        return e_failure;
    }
    fread(secret_data, secret_size, 1, fptr_secret);
    fclose(fptr_secret);

    /* ── Encode into pixel buffer ── */
    uint offset = 0;

    printf("INFO: Encoding magic string...\n");
    sleep(1);
    if (encode_bytes(&buf, &offset, (uint8_t *)MAGIC_STRING, 2) != e_success)
        goto fail;

    printf("INFO: Encoding extension size and data...\n");
    sleep(1);
    if (encode_uint32(&buf, &offset, ext_len) != e_success) goto fail;
    if (encode_bytes(&buf, &offset, (uint8_t *)ext, ext_len) != e_success) goto fail;

    printf("INFO: Encoding secret file size...\n");
    sleep(1);
    if (encode_uint32(&buf, &offset, secret_size) != e_success) goto fail;

    printf("INFO: Encoding secret data...\n");
    sleep(1);
    if (encode_bytes(&buf, &offset, secret_data, secret_size) != e_success) goto fail;

    free(secret_data);

    printf("INFO: Saving output image: %s\n", enc->stego_image_fname);
    sleep(1);
    if (save_image(enc->stego_image_fname, &buf) != e_success) {
        free_image(&buf);
        return e_failure;
    }

    free_image(&buf);
    printf("INFO: Encoding Done Successfully.\n");
    return e_success;

fail:
    free(secret_data);
    free_image(&buf);
    return e_failure;
}
