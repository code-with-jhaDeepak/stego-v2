#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "types.h"
#include <stdio.h>
#include <stdint.h>

/* Supported image formats */
typedef enum
{
    FORMAT_BMP,
    FORMAT_PNG,
    FORMAT_UNKNOWN
} ImageFormat;

/*
 * Unified pixel buffer — flat RGB byte array.
 * Both BMP and PNG are normalized into this before LSB ops.
 * BMP (bottom-up) and PNG (top-down) are both converted to top-down here.
 * Alpha channel (PNG RGBA) is stored separately — never touched by LSB logic.
 */
typedef struct
{
    uint8_t  *pixels;       /* Flat RGB buffer: [R,G,B, R,G,B, ...] */
    uint8_t  *alpha;        /* Alpha channel for RGBA PNG (NULL if RGB) */
    uint      width;
    uint      height;
    uint      channels;     /* 3 = RGB, 4 = RGBA */
    ImageFormat format;
} ImageBuffer;

/* Detect format from file extension */
ImageFormat detect_format(const char *filename);

/* Auto-detect first BMP or PNG in current directory */
/* Priority: BMP first, then PNG */
/* Returns allocated string (caller must free) or NULL if none found */
char *auto_detect_image(void);

/* Load image into flat pixel buffer */
/* Handles BMP bottom-up flip and PNG row pointer flattening */
Status load_image(const char *filename, ImageBuffer *buf);

/* Save pixel buffer back to image file */
/* Format determined by output filename extension */
Status save_image(const char *filename, const ImageBuffer *buf);

/* Free pixel buffer memory */
void free_image(ImageBuffer *buf);

/* Get total usable bytes for LSB encoding (RGB only, not alpha) */
uint get_image_capacity(const ImageBuffer *buf);

#endif
