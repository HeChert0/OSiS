CC = gcc
CFLAGS = -Wall -O0 -g
LDFLAGS = -lncursesw
OUTDIR = ../../build/debug
SRCS = main.c list.c restore.c purge.c browser.c
OBJS = $(SRCS:.c=.o)
TARGET = $(OUTDIR)/trashctl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c trashctl.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
