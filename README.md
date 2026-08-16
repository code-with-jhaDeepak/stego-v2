# stego-v2 — Image Steganography Tool in Pure C

A command-line tool written in **pure C** that hides secret text files inside **BMP and PNG images** using the **Least Significant Bit (LSB)** substitution technique. The encoded image is visually identical to the original — the hidden data is completely undetectable to the naked eye.

An upgrade over [Project_steganography](https://github.com/code-with-jhaDeepak/Project_steganography) with PNG support, a proper Makefile, and auto-detection of images in the current directory.

---

## What's New in v2

| Feature | v1 | v2 |
|---|---|---|
| BMP support | ✅ | ✅ |
| PNG support | ❌ | ✅ |
| Makefile | ❌ | ✅ |
| Auto-detect image | ❌ | ✅ |
| Fixed-endian size encoding | ❌ | ✅ |
| Alpha channel safety | ❌ | ✅ |
| Unified pixel buffer | ❌ | ✅ |

---

## How It Works

Each pixel in a BMP/PNG image is stored as RGB bytes. This tool replaces the **least significant bit** of 8 consecutive bytes to encode one character of secret data — making the visual change imperceptible (only 1 bit out of 8 changes per byte).

```
Original byte:  10110110
                        ^--- only this bit changes
Encoded byte:   10110111
```

A **magic string signature** (`#*`) is embedded at the start so the decoder can verify the image was actually encoded before attempting extraction.

**PNG handling:** libpng is used to decompress PNG pixel data into a flat RGB buffer before LSB manipulation. Alpha channels (RGBA PNG) are extracted separately and never touched — preventing any visible transparency glitches.

**BMP handling:** BMP stores rows bottom-up in BGR format. The loader flips rows to top-down and converts BGR→RGB so both formats share identical LSB logic.

---

## Project Structure

```
.
├── main.c        # Entry point — argument parsing, operation routing
├── encode.c      # Encoding logic using unified ImageBuffer
├── encode.h
├── decode.c      # Decoding logic using unified ImageBuffer
├── decode.h
├── image_io.c    # BMP & PNG load/save, auto-detect, unified pixel buffer
├── image_io.h
├── common.h      # Magic string definition
├── types.h       # Custom types (Status, OperationType)
├── Makefile      # Build system
└── diwali.png    # Sample image for testing
```

---

## Dependencies

```bash
sudo apt install libpng-dev pkgconf
```

---

## Build

```bash
make
```

Clean build artifacts:

```bash
make clean
```

---

## Usage

### Encode — hide secret data inside image

```bash
./stego -e <image.bmp|image.png> <secret.txt> [output]
```

**Examples:**
```bash
./stego -e diwali.png secret.txt output.png
./stego -e photo.bmp secret.txt output.bmp
```

If no output filename is given, defaults to `encrypted.bmp` or `encrypted.png`.

### Auto-detect mode — no image specified

```bash
./stego -e secret.txt
```

Tool scans the current directory, picks the first BMP or PNG it finds (BMP takes priority), and uses it automatically.

---

### Decode — extract hidden data from image

```bash
./stego -d <stego.bmp|stego.png> [output.txt]
```

**Examples:**
```bash
./stego -d output.png recovered.txt
./stego -d output.bmp recovered.txt
```

If no output filename is given, defaults to `decrypted.txt`.

### Auto-detect decode

```bash
./stego -d
./stego -d recovered.txt
```

---

## Sample Output

**Encode:**
```
INFO: Loading source image: diwali.png
INFO: Image loaded — 1024 x 768, 3 channels
INFO: Capacity OK — image can hold 294912 bytes, need 52 bytes
INFO: Encoding magic string...
INFO: Encoding extension size and data...
INFO: Encoding secret file size...
INFO: Encoding secret data...
INFO: Saving output image: output.png
INFO: Encoding Done Successfully.
```

**Decode:**
```
INFO: Loading stego image: output.png
INFO: Image loaded — 1024 x 768
INFO: Verifying magic string...
INFO: Magic string verified.
INFO: Decoding extension info...
INFO: Secret file extension: .txt
INFO: Decoding secret file size...
INFO: Secret data size: 38 bytes
INFO: Decoding secret data...
INFO: Decoding Done Successfully.
INFO: Secret written to: recovered.txt
```

---

## What Gets Encoded

All metadata is embedded in LSBs along with the secret data:

| Field | Size |
|---|---|
| Magic string (`#*`) | 2 bytes |
| File extension size | 4 bytes (big-endian int) |
| File extension (`.txt`) | variable |
| Secret file size | 4 bytes (big-endian int) |
| Secret file data | variable |

Size values are stored in **big-endian order** — platform independent, no endianness issues across machines.

---

## Capacity Check

Before encoding, the tool validates the image is large enough:

```
Required = (2 + 4 + ext_len + 4 + secret_size) × 8 pixel bytes
```

A 1024×768 image holds up to **294,912 usable bytes** — enough for large text files.

---

## Key Technical Concepts

- **LSB substitution** — 1 bit per pixel byte, 8 bytes per character
- **Unified pixel buffer** — BMP and PNG normalized to flat top-down RGB array before LSB ops
- **BMP bottom-up flip** — rows reversed and BGR→RGB converted on load
- **PNG row flattening** — libpng row pointers collapsed into flat buffer
- **Alpha channel isolation** — RGBA PNG alpha stored separately, never modified
- **setjmp/longjmp error handling** — mandatory libpng error recovery
- **Big-endian size encoding** — fixed byte order, works across platforms
- **Auto-detection** — scans directory for first BMP or PNG (BMP priority)

---

## Limitations

- Secret file must be `.txt`
- Output format must match input format (BMP→BMP, PNG→PNG)
- JPEG not supported — lossy compression destroys LSB data

---

## Author

**Deepak Kumar Jha**
Embedded Systems & IoT Engineer | C/C++ | OpenWrt | Linux
[LinkedIn](https://linkedin.com/in/jhakumardeepakk) • [GitHub](https://github.com/code-with-jhaDeepak)
