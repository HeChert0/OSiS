CC = gcc
CFLAGS = -Wall -O0 -g -Wno-format-truncation
LDFLAGS = -lncursesw
OUTDIR = ../../src/tt
SRCS = leak_test.c
OBJS = $(SRCS:.c=.o)
TARGET = $(OUTDIR)/trashctl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(OUTDIR)
	@unset LD_PRELOAD && $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


clean:
	rm -f $(OBJS) $(TARGET)
