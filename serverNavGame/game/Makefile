# ===== GAME MAKEFILE =====

include ../engine/.env

WINDOWS ?= 0
RELEASE ?= 0

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
           -I$(VK_SDK_PATH)/include \
           -g3 \
           -O0 \
           -fno-omit-frame-pointer \
           -MMD -MP

# -static bakes in the MinGW runtime, leaving vulkan-1.dll as the only non-Windows DLL
# HTTP goes through Windows' own WinHTTP, not curl
LDFLAGS = -static \
          -L../engine \
          -L$(WINDOWS_LIBS)/lib-mingw-w64 \
          -L$(VK_SDK_PATH)/Lib \
          -lvulkan_engine_windows \
          -lglfw3 \
          -lvulkan-1 \
          -lwinhttp \
          -lgdi32 \
          -luser32 \
          -lshell32

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

# RELEASE=1 is the build handed to players: optimised, no ASan, validation layers off
# Linux looks for the libraries packed beside it in lib/
ifeq ($(RELEASE),1)
CXXFLAGS := $(filter-out -g3 -O0 -fsanitize=address,$(CXXFLAGS)) -O2 -DNDEBUG
LDFLAGS := $(patsubst -lvulkan_engine%,-lvulkan_engine%_release,$(LDFLAGS)) -s
ifneq ($(WINDOWS),1)
LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib'
endif
OBJ_DIR := $(OBJ_DIR)-release
TARGET := $(subst game,game_release,$(TARGET))
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
ENGINE_LIB = ../engine/libvulkan_engine_windows.a
else
ENGINE_LIB = ../engine/libvulkan_engine.a
endif

ifeq ($(RELEASE),1)
ENGINE_LIB := $(ENGINE_LIB:.a=_release.a)
endif

$(ENGINE_LIB):
	$(MAKE) -C ../engine \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		WINDOWS=$(WINDOWS) \
		RELEASE=$(RELEASE) \
		VULKAN_SDK_PATH="$(VULKAN_SDK_PATH)"


# ---------------------------------------------------------------------------
# Link game
# ---------------------------------------------------------------------------

$(TARGET): $(OBJECTS) $(ENGINE_LIB)
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
	rm -f game_release game_release.exe