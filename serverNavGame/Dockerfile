# syntax=docker/dockerfile:1

# ---- build stage -----------------------------------------------------------
# Pinned to match libs/ubuntu-latest-gcc-Release/libGameAnalytics.a, which was
# built on a GitHub Actions ubuntu-latest runner. If linking fails with
# undefined symbols, try FROM ubuntu:22.04 instead - that prebuilt .a may
# predate the runner's move to 24.04.
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    pkg-config \
    libvulkan-dev \
    libglfw3-dev \
    libglm-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Both Makefiles `include .env` (game's via ../engine/.env) for VULKAN_SDK_PATH.
# That file lives on your machine pointing at a WSL path, so it won't exist in
# the build context; point it at the system Vulkan packages installed above.
RUN echo "VULKAN_SDK_PATH=/usr" > engine/.env

RUN make -C engine && make -C game

# ---- runtime stage ----------------------------------------------------------
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libvulkan1 \
    mesa-vulkan-drivers \
    vulkan-validationlayers \
    libglfw3 \
    libcurl4 \
    libssl3 \
    libasan8 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/game/game        ./game/game
COPY --from=build /src/models           ./models
COPY --from=build /src/shaders          ./shaders
COPY --from=build /src/sounds           ./sounds
COPY --from=build /src/textures         ./textures
COPY --from=build /src/maps             ./maps
COPY --from=build /src/rooms            ./rooms

# Software Vulkan (lavapipe) via mesa-vulkan-drivers. Not setting
# VK_ICD_FILENAMES here - the loader auto-discovers whatever ICD json
# files exist under /usr/share/vulkan/icd.d/, which is more robust than
# hardcoding a filename that shifts between Mesa versions.
ENV ASAN_OPTIONS=detect_leaks=0

ENTRYPOINT ["./game/game"]
