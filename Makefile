CC = gcc
CFLAGS = -Wall -g $(shell pkg-config --cflags ncurses)
LDLIBS = $(shell pkg-config --libs ncurses)

BUILD_DIR = build
TARGET = $(BUILD_DIR)/kiuru

SRCS = src/main.c src/buffer.c src/renderer.c src/input.c src/help.c src/explorer.c src/manual.c src/util.c
OBJS = $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDLIBS)
	@echo "Ready: $(TARGET)"

$(BUILD_DIR)/%.o: src/%.c src/editor.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
