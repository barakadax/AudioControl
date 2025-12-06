CC = gcc
PKG_CONFIG = pkg-config

# Dependencies
DEPS_GTK = gtk4
DEPS_PULSE = libpulse libpulse-mainloop-glib
DEPS_X11 = x11

# Flags
CFLAGS := $(shell $(PKG_CONFIG) --cflags $(DEPS_GTK) $(DEPS_PULSE))
LIBS := $(shell $(PKG_CONFIG) --libs $(DEPS_GTK) $(DEPS_PULSE)) -lX11

# Source files
SRCS = src/main.c src/window.c src/callbacks.c src/process.c src/audio.c
TARGET = bam

.PHONY: all check_deps clean

all: check_deps $(TARGET)

check_deps:
	@which $(CC) > /dev/null || (echo "Error: $(CC) not found. Please install build-essential."; exit 1)
	@which $(PKG_CONFIG) > /dev/null || (echo "Error: pkg-config not found. Please install pkg-config."; exit 1)
	@$(PKG_CONFIG) --exists $(DEPS_GTK) || (echo "Error: $(DEPS_GTK) not found. Please install libgtk-4-dev."; exit 1)
	@$(PKG_CONFIG) --exists $(DEPS_PULSE) || (echo "Error: One or more PulseAudio dependencies missing. Please install libpulse-dev."; exit 1)
	@$(PKG_CONFIG) --exists $(DEPS_X11) || (echo "Warning: X11 pkg-config not found. Checking for library..."; ldconfig -p | grep libX11 > /dev/null || echo "Error: libX11 not found. Please install libx11-dev."; )
	@echo "All dependencies found."

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
