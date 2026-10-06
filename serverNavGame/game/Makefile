# ===== GAME MAKEFILE =====

include ../engine/.env

WINDOWS ?= 0

CXX ?= $(CROSS_COMPILE)g++

SRC_DIR = src

ifeq ($(WINDOWS),1)

# ---------------------------------------------------------------------------
# Windows build
# ---------------------------------------------------------------------------

WINDOWS_LIBS = ../engine/libs/windows

CXXFLAGS = -std=c++17 \
           -Isrc \
           -I../engine/include \
           -I../engine/libs/windows/include \
           -I$(VULKAN_SDK_PATH)/include \
           -g3 \
           -O0 \
           -fno-omit-frame-pointer \
           -MMD -MP

LDFLAGS = -L../engine \
          -L$(WINDOWS_LIBS)/lib \
          -L$(WINDOWS_LIBS)/GameAnalytics \
          -L$(VULKAN_SDK_PATH)/lib \
          -lvulkan_engine_windows \
          -lglfw3 \
          -lvulkan-1 \
          -lGameAnalytics \
          -lcurl \
          -lssl \
          -lcrypto \
          -lws2_32 \
          -lgdi32 \
          -luser32 \
          -lshell32 \
          -lcrypt32 \
          -lbcrypt

OBJ_DIR = obj/windows
TARGET = game.exe

else

# ---------------------------------------------------------------------------
# Linux build
# ---------------------------------------------------------------------------

LINUX_LIBS = ../engine/libs/linux

CXXFLAGS = -std=c++17 \
           -Isrc \
           -I../engine/include \
           -I../engine/libs/linux/GameAnalytics \
           -I../engine/libs/linux/cgltf \
           -I../engine/libs/linux/miniaudio \
           -I../engine/libs/linux/tiny_obj_loader \
           -I$(VULKAN_SDK_PATH)/include \
           -g3 \
           -O0 \
           -fno-omit-frame-pointer \
           -fsanitize=address \
           -MMD -MP

LDFLAGS = -L../engine \
          -lvulkan_engine \
          -L$(VULKAN_SDK_PATH)/lib \
          `pkg-config --libs glfw3` \
          -lvulkan \
          -L$(LINUX_LIBS)/ubuntu-latest-gcc-Release \
          -lGameAnalytics \
          -lcurl \
          -lssl \
          -lcrypto \
          -lpthread \
          -ldl \
          -lm

CXXFLAGS += `pkg-config --cflags libcurl`
LDFLAGS += `pkg-config --libs libcurl`

OBJ_DIR = obj/linux
TARGET = game

endif


SOURCES = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS    = $(OBJECTS:.o=.d)

all: $(TARGET)

# Automatically rebuild objects when included headers change.
-include $(DEPS)


# ---------------------------------------------------------------------------
# Engine dependency
# ---------------------------------------------------------------------------

ifeq ($(WINDOWS),1)

../engine/libvulkan_engine_windows.a:
	$(MAKE) -C ../engine \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		WINDOWS=1 \
		VULKAN_SDK_PATH="$(VULKAN_SDK_PATH)"

else

../engine/libvulkan_engine.a:
	$(MAKE) -C ../engine \
		WINDOWS=0 \
		VULKAN_SDK_PATH="$(VULKAN_SDK_PATH)"

endif


# ---------------------------------------------------------------------------
# Link game
# ---------------------------------------------------------------------------

$(TARGET): $(OBJECTS) $(if $(filter 1,$(WINDOWS)),../engine/libvulkan_engine_windows.a,../engine/libvulkan_engine.a)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)


# ---------------------------------------------------------------------------
# Compile game source
# ---------------------------------------------------------------------------

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


.PHONY: all clean

clean:
	rm -rf obj
	rm -f game
	rm -f game.exe