CC = gcc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags ncurses)
LDLIBS = $(shell pkg-config --libs ncurses)
BUILD = build

all: $(BUILD)/editor

$(BUILD)/editor: editor.c editor.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) editor.c -o $(BUILD)/editor $(LDLIBS)

clean:
	rm -rf $(BUILD)

