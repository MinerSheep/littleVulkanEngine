# ===== GAME MAKEFILE =====

include ../engine/.env

CXX = g++
CXXFLAGS = -std=c++17 -Isrc -I../engine/include -I$(VULKAN_SDK_PATH)/include -g3 -O0 -fno-omit-frame-pointer -fsanitize=address -MMD -MP
LDFLAGS = -L../engine -lvulkan_engine \
          -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

CFLAGS += `pkg-config --cflags libcurl`
LDFLAGS += `pkg-config --libs libcurl`

SRC_DIR = src
OBJ_DIR = obj

SOURCES = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS    = $(OBJECTS:.o=.d)

TARGET = game

all: $(TARGET)

# 🔥 NEW: Tell Make how to rebuild the engine
../engine/libvulkan_engine.a:
	$(MAKE) -C ../engine

$(TARGET): $(OBJECTS) ../engine/libvulkan_engine.a
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
