include .env


CFLAGS = -std=c++17 -I. -Ilibs -Iinclude -I$(VULKAN_SDK_PATH)/include -MMD -MP
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

SRC_DIR = src
OBJ_DIR = obj

SOURCES = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS    = $(OBJECTS:.o=.d)

TARGET = libvulkan_engine.a

all: $(TARGET)

# Editing a header rebuilds every .o that includes it, the same way game/Makefile
# already works. The old rule was "$(TARGET): $(SOURCES)" with the header list
# commented out, so a header edit rebuilt nothing at all: the engine kept using the
# old struct layouts while the game rebuilt against the new ones, and the two halves
# disagreed on where members lived (see CLAUDE.md's "Critical build gotcha")
#
# The old HEADERS line looked for .hpp under src/, but the engine keeps its headers
# in include/, so it found nothing and would not have helped even if it had been used
-include $(DEPS)

# One .o per source, keeping the src/ subdirectory under obj/ so two sources may
# share a basename. The old rule compiled every source in one command into a flat
# pile in engine/, then archived with "ar rcs *.o", which also swept up orphaned
# .o files left behind by sources that had since been renamed or deleted
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	g++ $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	ar rcs $@ $(OBJECTS)

.PHONY: all test clean

test: a.out
	./a.out

clean:
	rm -rf $(OBJ_DIR)
	rm -f *.o
	rm -f libvulkan_engine.a
