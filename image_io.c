#include "image_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <png.h>

/* ─── Format detection ─────────────────────────────────────────────────── */

ImageFormat detect_format(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return FORMAT_UNKNOWN;

    if (strcmp(ext, ".bmp") == 0 || strcmp(ext, ".BMP") == 0)
        return FORMAT_BMP;
    if (strcmp(ext, ".png") == 0 || strcmp(ext, ".PNG") == 0)
        return FORMAT_PNG;

    return FORMAT_UNKNOWN;
}

/* ─── Auto-detect image in current directory ───────────────────────────── */

char *auto_detect_image(void)
{
    DIR *dir = opendir(".");
    if (!dir) return NULL;

    struct dirent *entry;
    char *bmp_found = NULL;
    char *png_found = NULL;

    while ((entry = readdir(dir)) != NULL)
    {
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;

        if ((strcmp(ext, ".bmp") == 0 || strcmp(ext, ".BMP") == 0) && !bmp_found)
            bmp_found = strdup(entry->d_name);

        if ((strcmp(ext, ".png") == 0 || strcmp(ext, ".PNG") == 0) && !png_found)
            png_found = strdup(entry->d_name);
    }
    closedir(dir);

    /* BMP takes priority */
    if (bmp_found) {
        free(png_found);
        return bmp_found;
    }
    return png_found; /* may be NULL if nothing found */
}

/* ─── BMP loader ───────────────────────────────────────────────────────── */

static Status load_bmp(const char *filename, ImageBuffer *buf)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open BMP file: %s\n", filename);
        return e_failure;
    }

    /* Read width and height from BMP header (offsets 18 and 22) */
    /* Height is signed in BMP spec — negative means top-down stored */
    uint32_t width;
    int32_t  height_signed;
    fseek(fp, 18, SEEK_SET);
    fread(&width,        sizeof(uint32_t), 1, fp);
    fread(&height_signed, sizeof(int32_t),  1, fp);
    uint32_t height = (uint32_t)(height_signed < 0 ? -height_signed : height_signed);
    int bmp_top_down = (height_signed < 0); /* negative = already top-down */

    /* BMP row size is padded to 4-byte boundary */
    uint row_stride = ((width * 3 + 3) / 4) * 4;

    buf->width    = width;
    buf->height   = height;
    buf->channels = 3;
    buf->format   = FORMAT_BMP;
    buf->alpha    = NULL;
    buf->pixels   = (uint8_t *)malloc(width * height * 3);
    if (!buf->pixels) {
        fclose(fp);
        fprintf(stderr, "ERROR: malloc failed for BMP pixel buffer\n");
        return e_failure;
    }

    uint8_t *row_buf = (uint8_t *)malloc(row_stride);
    if (!row_buf) {
        free(buf->pixels);
        fclose(fp);
        return e_failure;
    }

    /* BMP pixel data starts at offset stored at byte 10 */
    uint32_t pixel_offset;
    fseek(fp, 10, SEEK_SET);
    fread(&pixel_offset, sizeof(uint32_t), 1, fp);
    fseek(fp, pixel_offset, SEEK_SET);

    /*
     * BMP stores rows bottom-up (unless height is negative = top-down).
     * We normalize to top-down so LSB logic is consistent with PNG.
     */
    for (uint r = 0; r < height; r++)
    {
        fread(row_buf, row_stride, 1, fp);
        /* Map file row to buffer row */
        uint dst_row = bmp_top_down ? r : (height - 1 - r);
        uint8_t *dst = buf->pixels + dst_row * width * 3;
        for (uint col = 0; col < width; col++)
        {
            /* BMP is BGR, convert to RGB */
            dst[col * 3 + 0] = row_buf[col * 3 + 2]; /* R */
            dst[col * 3 + 1] = row_buf[col * 3 + 1]; /* G */
            dst[col * 3 + 2] = row_buf[col * 3 + 0]; /* B */
        }
    }

    free(row_buf);
    fclose(fp);
    return e_success;
}

/* ─── BMP saver ────────────────────────────────────────────────────────── */

static Status save_bmp(const char *filename, const ImageBuffer *buf)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open output BMP: %s\n", filename);
        return e_failure;
    }

    uint width     = buf->width;
    uint height    = buf->height;
    uint row_stride = ((width * 3 + 3) / 4) * 4;
    uint pixel_data_size = row_stride * height;
    uint file_size = 54 + pixel_data_size;

    /* BMP file header (14 bytes) */
    uint8_t file_hdr[14] = {
        'B','M',
        (uint8_t)(file_size),       (uint8_t)(file_size >> 8),
        (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24),
        0,0, 0,0,   /* reserved */
        54,0,0,0    /* pixel data offset */
    };
    fwrite(file_hdr, 14, 1, fp);

    /* DIB header (40 bytes) — write field by field, little-endian */
    uint8_t dib[40] = {0};
    /* BITMAPINFOHEADER size = 40 */
    dib[0]=40; dib[1]=0; dib[2]=0; dib[3]=0;
    /* width (4 bytes LE) */
    dib[4]=(uint8_t)(width);       dib[5]=(uint8_t)(width>>8);
    dib[6]=(uint8_t)(width>>16);   dib[7]=(uint8_t)(width>>24);
    /* height (4 bytes LE, positive = bottom-up) */
    dib[8]=(uint8_t)(height);      dib[9]=(uint8_t)(height>>8);
    dib[10]=(uint8_t)(height>>16); dib[11]=(uint8_t)(height>>24);
    /* color planes = 1 */
    dib[12]=1; dib[13]=0;
    /* bits per pixel = 24 */
    dib[14]=24; dib[15]=0;
    /* compression = 0 (none) */
    dib[16]=0; dib[17]=0; dib[18]=0; dib[19]=0;
    /* image size */
    dib[20]=(uint8_t)(pixel_data_size);      dib[21]=(uint8_t)(pixel_data_size>>8);
    dib[22]=(uint8_t)(pixel_data_size>>16);  dib[23]=(uint8_t)(pixel_data_size>>24);
    /* X/Y pixels per meter = 2835 (~72 DPI) */
    dib[24]=0x13; dib[25]=0x0B; dib[26]=0; dib[27]=0;
    dib[28]=0x13; dib[29]=0x0B; dib[30]=0; dib[31]=0;
    /* colors in table = 0, important colors = 0 */
    fwrite(dib, 40, 1, fp);

    /* Write pixel rows bottom-up, converting RGB→BGR */
    uint8_t *row_buf = (uint8_t *)calloc(row_stride, 1);
    for (int row = (int)height - 1; row >= 0; row--)
    {
        const uint8_t *src = buf->pixels + row * width * 3;
        for (uint col = 0; col < width; col++)
        {
            row_buf[col * 3 + 0] = src[col * 3 + 2]; /* B */
            row_buf[col * 3 + 1] = src[col * 3 + 1]; /* G */
            row_buf[col * 3 + 2] = src[col * 3 + 0]; /* R */
        }
        fwrite(row_buf, row_stride, 1, fp);
    }

    free(row_buf);
    fclose(fp);
    return e_success;
}

/* ─── PNG loader ───────────────────────────────────────────────────────── */

static Status load_png(const char *filename, ImageBuffer *buf)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open PNG file: %s\n", filename);
        return e_failure;
    }

    /* Validate PNG signature */
    uint8_t sig[8];
    fread(sig, 8, 1, fp);
    if (png_sig_cmp(sig, 0, 8)) {
        fprintf(stderr, "ERROR: Not a valid PNG file: %s\n", filename);
        fclose(fp);
        return e_failure;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return e_failure; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(fp); return e_failure; }

    /* libpng error handling — mandatory setjmp */
    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "ERROR: libpng failed reading %s\n", filename);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return e_failure;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8); /* already read 8 bytes */
    png_read_info(png, info);

    uint width    = png_get_image_width(png, info);
    uint height   = png_get_image_height(png, info);
    uint8_t color_type = png_get_color_type(png, info);
    uint8_t bit_depth  = png_get_bit_depth(png, info);

    /* Normalize to 8-bit RGB or RGBA */
    if (bit_depth == 16)        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    uint channels = png_get_channels(png, info);
    buf->width    = width;
    buf->height   = height;
    buf->channels = channels;
    buf->format   = FORMAT_PNG;
    buf->pixels   = (uint8_t *)malloc(width * height * 3);
    buf->alpha    = (channels == 4) ? (uint8_t *)malloc(width * height) : NULL;

    if (!buf->pixels) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return e_failure;
    }

    /* Read row by row into flat buffer, separate alpha */
    uint8_t *row_buf = (uint8_t *)malloc(width * channels);
    for (uint row = 0; row < height; row++)
    {
        png_read_row(png, row_buf, NULL);
        uint8_t *rgb_dst = buf->pixels + row * width * 3;
        for (uint col = 0; col < width; col++)
        {
            rgb_dst[col * 3 + 0] = row_buf[col * channels + 0]; /* R */
            rgb_dst[col * 3 + 1] = row_buf[col * channels + 1]; /* G */
            rgb_dst[col * 3 + 2] = row_buf[col * channels + 2]; /* B */
            if (channels == 4 && buf->alpha)
                buf->alpha[row * width + col] = row_buf[col * channels + 3];
        }
    }

    free(row_buf);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return e_success;
}

/* ─── PNG saver ────────────────────────────────────────────────────────── */

static Status save_png(const char *filename, const ImageBuffer *buf)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "ERROR: Cannot open output PNG: %s\n", filename);
        return e_failure;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(fp); return e_failure; }

    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); fclose(fp); return e_failure; }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "ERROR: libpng failed writing %s\n", filename);
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return e_failure;
    }

    png_init_io(png, fp);

    uint channels   = buf->channels;
    int color_type  = (channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png, info, buf->width, buf->height, 8,
                 color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    uint8_t *row_buf = (uint8_t *)malloc(buf->width * channels);
    for (uint row = 0; row < buf->height; row++)
    {
        const uint8_t *rgb_src = buf->pixels + row * buf->width * 3;
        for (uint col = 0; col < buf->width; col++)
        {
            row_buf[col * channels + 0] = rgb_src[col * 3 + 0];
            row_buf[col * channels + 1] = rgb_src[col * 3 + 1];
            row_buf[col * channels + 2] = rgb_src[col * 3 + 2];
            if (channels == 4 && buf->alpha)
                row_buf[col * channels + 3] = buf->alpha[row * buf->width + col];
        }
        png_write_row(png, row_buf);
    }

    free(row_buf);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return e_success;
}

/* ─── Public API ───────────────────────────────────────────────────────── */

Status load_image(const char *filename, ImageBuffer *buf)
{
    ImageFormat fmt = detect_format(filename);
    if (fmt == FORMAT_BMP) return load_bmp(filename, buf);
    if (fmt == FORMAT_PNG) return load_png(filename, buf);
    fprintf(stderr, "ERROR: Unsupported format: %s\n", filename);
    return e_failure;
}

Status save_image(const char *filename, const ImageBuffer *buf)
{
    ImageFormat fmt = detect_format(filename);
    if (fmt == FORMAT_BMP) return save_bmp(filename, buf);
    if (fmt == FORMAT_PNG) return save_png(filename, buf);
    fprintf(stderr, "ERROR: Unsupported format: %s\n", filename);
    return e_failure;
}

void free_image(ImageBuffer *buf)
{
    if (buf->pixels) { free(buf->pixels); buf->pixels = NULL; }
    if (buf->alpha)  { free(buf->alpha);  buf->alpha  = NULL; }
}

uint get_image_capacity(const ImageBuffer *buf)
{
    /* Only RGB bytes usable — 3 bytes per pixel */
    return buf->width * buf->height * 3;
}
