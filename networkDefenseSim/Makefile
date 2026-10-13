.PHONY: all clean

CXX = g++

CXXFLAGS = -std=c++17 \
           -Isrc \
           -Iinclude \
           -g3 \
           -O0 \
           -fsanitize=address

BUILD_DIR = build

SRCS := $(wildcard *.cpp)

all: $(BUILD_DIR)/sender $(BUILD_DIR)/receiver

$(BUILD_DIR)/sender: sender.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/receiver: receiver.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR)