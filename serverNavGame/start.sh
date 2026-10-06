#!/usr/bin/env bash
# Brings up a virtual display + VNC + noVNC inside the container, then runs
# the game against it. No X server or VNC client needs to be installed on
# the host - just open a browser to http://localhost:6080/vnc.html
set -eu

SCREEN="${SCREEN:-800x600x24}"
export DISPLAY=":99"

Xvfb "$DISPLAY" -screen 0 "$SCREEN" &
XVFB_PID=$!

# Give Xvfb a moment to come up before x11vnc tries to attach to it
sleep 1

x11vnc -display "$DISPLAY" -nopw -forever -shared -quiet &
X11VNC_PID=$!

websockify --web=/usr/share/novnc 6080 localhost:5900 &
NOVNC_PID=$!

cleanup() {
  kill "$NOVNC_PID" "$X11VNC_PID" "$XVFB_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo ">> open http://localhost:6080/vnc.html in a browser to see the window"

./game/game
