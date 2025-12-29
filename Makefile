CC = gcc
CFLAGS = -Wall -g $(shell pkg-config --cflags ncurses)
LDLIBS = $(shell pkg-config --libs ncurses)

BUILD_DIR = build
TARGET = $(BUILD_DIR)/editor

SRCS = main.c buffer.c renderer.c input.c help.c explorer.c manual.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDLIBS)
	@echo "Ready: $(TARGET)"

$(BUILD_DIR)/%.o: %.c editor.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
