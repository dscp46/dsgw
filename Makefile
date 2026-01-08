CC      ?= gcc
CFLAGS  += -Wall

SRC_DIR := src
BIN_DIR := bin

PREFIX  ?= /usr
BINDIR  := $(PREFIX)/bin

SRCS := main.c config.c dextra.c dextra_peer.c kiss.c
OBJS := $(SRCS:%.c=$(BIN_DIR)/%.o)

TARGET := $(BIN_DIR)/dsgwd

.PHONY: all clean install uninstall

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/dsgwd

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/dsgwd

clean:
	rm -rf $(BIN_DIR)

