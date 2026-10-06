# ---- build stage -----------------------------------------------------------
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

RUN echo "VULKAN_SDK_PATH=/usr" > engine/.env

RUN make -C engine
RUN make -C game


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
    libasound2t64 \
    libpulse0 \
    ca-certificates \
    xvfb \
    x11vnc \
    novnc \
    websockify \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/game/game        ./game/game
COPY --from=build /src/models           ./models
COPY --from=build /src/shaders          ./shaders
COPY --from=build /src/sounds           ./sounds
COPY --from=build /src/textures         ./textures
COPY --from=build /src/maps             ./maps
COPY --from=build /src/rooms            ./rooms
COPY start.sh ./start.sh
RUN chmod +x ./start.sh

ENV ASAN_OPTIONS=detect_leaks=0

# noVNC's web UI - open http://localhost:6080/vnc.html after `docker run`.
# No DISPLAY/WAYLAND_DISPLAY/XDG_RUNTIME_DIR/PULSE_SERVER are set here on
# purpose: those WSLg paths only exist on the machine that built this image
# and don't carry over to anyone else's Windows install. start.sh creates
# its own display (Xvfb) instead, so the image needs nothing from the host.
EXPOSE 6080

ENTRYPOINT ["./start.sh"]
