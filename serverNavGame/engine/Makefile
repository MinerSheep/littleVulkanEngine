include .env


CFLAGS = -std=c++17 -I. -Ilibs -Iinclude -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

# Source files
SOURCES = $(shell find src -type f -name "*.cpp")
HEADERS = $(shell find src -type f -name "*.hpp")

TARGET = libvulkan_engine.a

# $(HEADERS) $(vertObjFiles) $(fragObjFiles)
$(TARGET): $(SOURCES)
	g++ $(CFLAGS) -c $(SOURCES) $(LDFLAGS)
	ar rcs $(TARGET) *.o

.PHONY: test clean

test: a.out
	./a.out

clean:
	rm -f *.o
	rm -f libvulkan_engine.a