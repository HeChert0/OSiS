CC       := gcc
CFLAGS   := -Wall -Wextra -std=c11 -pedantic -MMD -MP \
            -g -Wno-unused-parameter -Wno-unused-variable \
            -D_POSIX_C_SOURCE=200809L
LDFLAGS += -lm

SRCDIR    := src
BUILDDIR  := build
DBGDIR    := $(BUILDDIR)/debug
RELDIR    := $(BUILDDIR)/release

BINS      := gen view sort_index
DBG_BINS  := $(addprefix $(DBGDIR)/, $(BINS))
REL_BINS  := $(addprefix $(RELDIR)/, $(BINS))

.PHONY: all debug release clean directories

all: debug

debug: CFLAGS += -O0
debug: directories $(DBG_BINS)

release: CFLAGS += -O3 -DNDEBUG
release: directories $(REL_BINS)

$(DBGDIR)/gen: $(SRCDIR)/gen.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(DBGDIR)/view: $(SRCDIR)/view.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(DBGDIR)/sort_index: $(SRCDIR)/sort_index.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(RELDIR)/gen: $(SRCDIR)/gen.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(RELDIR)/view: $(SRCDIR)/view.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(RELDIR)/sort_index: $(SRCDIR)/sort_index.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

directories:
	@mkdir -p $(DBGDIR) $(RELDIR)

clean:
	-rm -rf $(BUILDDIR)
	-rm data.idx

-include $(wildcard $(SRCDIR)/*.d)
