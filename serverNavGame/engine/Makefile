include .env

WINDOWS ?= 0

CXX ?= $(CROSS_COMPILE)g++
AR  ?= $(CROSS_COMPILE)ar

SRC_DIR = src

ifeq ($(WINDOWS),1)

# ---------------------------------------------------------------------------
# Windows build
# ---------------------------------------------------------------------------

WINDOWS_LIBS = libs/windows

CXXFLAGS = -std=c++17 \
           -I. \
           -Ilibs \
           -Iinclude \
           -I$(WINDOWS_LIBS)/include \
           -I$(VK_SDK_PATH)/Include \
           -MMD -MP

LDFLAGS = -L$(WINDOWS_LIBS)/windows-2022-cl-Release \
		  -L$(WINDOWS_LIBS)/lib-mingw-w64 \
          -L$(VK_SDK_PATH)/lib \
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
TARGET = libvulkan_engine_windows.a

else

# ---------------------------------------------------------------------------
# Linux build
# ---------------------------------------------------------------------------

LINUX_LIBS = libs/linux

CXXFLAGS = -std=c++17 \
           -I. \
           -Ilibs \
           -Iinclude \
           -I$(VULKAN_SDK_PATH)/include \
           -MMD -MP

LDFLAGS = -L$(LINUX_LIBS)/ubuntu-latest-gcc-Release \
          -L$(VULKAN_SDK_PATH)/lib \
          `pkg-config --static --libs glfw3` \
          -lvulkan \
          -lGameAnalytics \
          -lcurl \
          -lssl \
          -lcrypto \
          -lpthread \
          -ldl

OBJ_DIR = obj/linux
TARGET = libvulkan_engine.a

endif


SOURCES = $(shell find $(SRC_DIR) -type f -name "*.cpp")
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS    = $(OBJECTS:.o=.d)

all: $(TARGET)

# Automatically rebuild objects when included headers change.
-include $(DEPS)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)


.PHONY: all test clean

test: $(TARGET)
	$(CXX) $(CXXFLAGS) src/test_main.cpp $(TARGET) $(LDFLAGS) -o a.out
	./a.out


clean:
	rm -rf obj
	rm -f *.o
	rm -f libvulkan_engine.a
	rm -f libvulkan_engine_windows.a