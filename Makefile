CC := gcc
CFLAGS := -ggdb3 -O2 -Wall -std=c11
CFLAGS += -Wno-unused-function -Wvla

# Flags for FUSE
LDLIBS := $(shell pkg-config fuse --cflags --libs)

SRCDIR := src
OBJDIR := obj
BINDIR := .
FS_NAME := $(BINDIR)/fisopfs

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

all: build

build: $(FS_NAME)

format: .clang-files .clang-format
	xargs -r clang-format -i <$<

clean:
	rm -rf $(BINDIR)/*.o $(FS_NAME)

run: build
	./$(FS_NAME) -f ./mount/

$(FS_NAME): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) $(BINDIR):
	mkdir -p $@

.PHONY: all build clean format

