CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config --cflags libpng)
LIBS    = $(shell pkg-config --libs libpng)
TARGET  = stego
SRCS    = main.c encode.c decode.c image_io.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: check-libpng $(TARGET)

check-libpng:
	@pkg-config --exists libpng || \
		(echo "ERROR: libpng not found. Run: sudo apt install libpng-dev" && exit 1)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "Build successful: ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) encrypted.bmp encrypted.png decrypted.txt
	@echo "Cleaned."
