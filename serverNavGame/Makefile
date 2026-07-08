# ===== ROOT MAKEFILE =====

.PHONY: all engine game clean

# Default target: build everything
all: engine game

# ----- SHADERS (NEW) -----
shaders:
	@echo "Compiling shaders..."
	@find shaders -type f -name "*.vert" -exec glslangValidator -V {} -o {}.spv \;
	@find shaders -type f -name "*.frag" -exec glslangValidator -V {} -o {}.spv \;

# ----- ENGINE BUILD -----
engine:
	@echo "Cleaning engine build..."
	$(MAKE) -C engine clean
	@echo "Building engine..."
	$(MAKE) -C engine

# ----- GAME BUILD -----
game:
	@echo "Cleaning game build..."
	$(MAKE) -C game clean
	@echo "Building game..."
	$(MAKE) -C game

# ----- CLEAN EVERYTHING -----
clean:
	@echo "Cleaning engine..."
	$(MAKE) -C engine clean
	@echo "Cleaning game..."
	$(MAKE) -C game clean
