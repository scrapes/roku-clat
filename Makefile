SRCDIR := src
INCDIR := inc
OBJDIR := obj
BINDIR := bin

SRCEXT := c
DEPEXT := h
OBJEXT := o

CC     := gcc
CFLAGS := -std=gnu11 -Wall -Werror -O2 -g
INC    := -I$(INCDIR)
LIB    := -static

TARGET := roku

SRCS   := $(wildcard $(SRCDIR)/*.$(SRCEXT) $(SRCDIR)/**/*.$(SRCEXT))
DEPS   := $(wildcard $(INCDIR)/*.$(DEPEXT) $(INCDIR)/**/*.$(DEPEXT))
OBJS   := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.$(SRCEXT)=.$(OBJEXT)))
BIN    := $(BINDIR)/$(TARGET)

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

.PHONY: all clean docker-build macos-build

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) -static -o $(BIN) $^ $(LIB)

$(OBJDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) $(DEPS)
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Build for Linux using Docker (creates Linux binary)
docker-build:
	@echo "Building roku-clat for Linux in Docker container..."
	docker run --rm -v $(PWD):/workspace -w /workspace docker.io/library/gcc:latest make clean all

clean:
	$(RM) -r $(BINDIR) $(OBJDIR)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

