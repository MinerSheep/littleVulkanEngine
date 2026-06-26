include .env


CFLAGS = -std=c++17 -I. -Ilibs -Isrc -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

# Source files
SOURCES = $(shell find src -type f -name "*.cpp")
HEADERS = $(shell find src -type f -name "*.hpp")

# create list of all spv files and set as dependency
vertSources = $(shell find shaders -type f -name "*.vert")
vertObjFiles = $(patsubst %.vert, %.vert.spv, $(vertSources))
fragSources = $(shell find shaders -type f -name "*.frag")
fragObjFiles = $(patsubst %.frag, %.frag.spv, $(fragSources))

TARGET = libvulkan_engine.a

# $(HEADERS) $(vertObjFiles) $(fragObjFiles)
$(TARGET): $(SOURCES)
	g++ $(CFLAGS) -c $(SOURCES) $(LDFLAGS)
	ar rcs $(TARGET) *.o

# make shader targets
%.spv: %
	glslangValidator -V $< -o $@

.PHONY: test clean

test: a.out
	./a.out

clean:
	rm -f a.out
	rm -f shaders/*.spv