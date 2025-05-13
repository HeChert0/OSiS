CC = gcc
CFLAGS = -Wall -fPIC -O0 -g
LDFLAGS = -shared
OUTDIR = ../../build/debug
TARGET = $(OUTDIR)/libtrash.so

all: $(TARGET)

$(TARGET): trash_unlink.c
	@unset LD_PRELOAD && $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
