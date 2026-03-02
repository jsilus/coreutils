CC = gcc
CFLAGS = -Wall -std=c99

SRC_DIR = src
BUILD_DIR = build

PROGRAMS = arch base64

.PHONY: all clean $(PROGRAMS)

all: $(addprefix $(BUILD_DIR)/, $(PROGRAMS))

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(PROGRAMS): %: $(BUILD_DIR)/%

$(BUILD_DIR)/%: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)
