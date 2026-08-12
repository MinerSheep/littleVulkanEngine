#!/usr/bin/env python3
"""Synthesises the placeholder clips the spooky events ask for.

Same spirit as the existing lever.wav and stone.wav -- nothing here is meant to
ship, it just means the events make a noise instead of warning on startup.

Run it from the repo root:

    python tools/make_sounds.py

It only writes a clip that is not already on disk, so a real recording dropped
into sounds/ is never overwritten. Pass --force to rebuild everything.
"""

import argparse
import math
import os
import random
import struct
import wave

RATE = 44100


def write(path, samples):
    """Writes mono 16 bit PCM, clipped rather than wrapped"""
    frames = bytearray()
    for value in samples:
        clamped = max(-1.0, min(1.0, value))
        frames += struct.pack("<h", int(clamped * 32000))

    with wave.open(path, "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        handle.writeframes(bytes(frames))


def envelope(i, total, attack, release):
    """Fades a clip in and back out so it never clicks at the edges"""
    up = min(1.0, i / max(1.0, attack * RATE))
    down = min(1.0, (total - i) / max(1.0, release * RATE))
    return up * down


def water(seconds=4.0):
    """Filtered noise, the sound of a tap nobody can find"""
    total = int(RATE * seconds)
    out = []
    low = 0.0
    band = 0.0
    for i in range(total):
        noise = random.uniform(-1.0, 1.0)
        low += 0.06 * (noise - low)
        band += 0.35 * (noise - band)
        wobble = 0.5 + 0.5 * math.sin(2.0 * math.pi * 0.7 * i / RATE)
        out.append((low * 2.2 + band * 0.25 * wobble) * 0.5 * envelope(i, total, 0.25, 0.6))
    return out


def piano(seconds=6.0):
    """A slow three note figure, struck and left to ring"""
    notes = [(0.0, 261.63), (1.1, 311.13), (2.3, 233.08), (3.6, 174.61)]
    total = int(RATE * seconds)
    out = [0.0] * total

    for start, freq in notes:
        begin = int(start * RATE)
        for i in range(begin, total):
            age = (i - begin) / RATE
            decay = math.exp(-1.4 * age)
            if decay < 0.001:
                break
            phase = 2.0 * math.pi * freq * age
            # A little of the octave above it, so it is not a bare sine
            out[i] += (math.sin(phase) + 0.32 * math.sin(2.0 * phase) +
                       0.12 * math.sin(3.0 * phase)) * decay * 0.22

    return [v * envelope(i, total, 0.01, 0.8) for i, v in enumerate(out)]


def footsteps(seconds=10.0):
    """Somebody walking in the dark, unhurried and not quite even"""
    total = int(RATE * seconds)
    out = [0.0] * total
    step = 0.62
    when = 0.35

    while when < seconds - 0.2:
        begin = int(when * RATE)
        length = int(0.09 * RATE)
        loudness = random.uniform(0.45, 0.75)
        for i in range(begin, min(begin + length, total)):
            age = (i - begin) / RATE
            body = math.sin(2.0 * math.pi * 95.0 * age) * math.exp(-26.0 * age)
            grit = random.uniform(-1.0, 1.0) * math.exp(-55.0 * age)
            out[i] += (body * 0.8 + grit * 0.35) * loudness

        # An even tread would read as a machine, so each one lands a little off
        when += step + random.uniform(-0.05, 0.05)

    return [v * envelope(i, total, 0.05, 0.4) for i, v in enumerate(out)]


def drone(seconds=3.0):
    """Low swell for the thing that passes over the greenhouse"""
    total = int(RATE * seconds)
    out = []
    for i in range(total):
        age = i / RATE
        slide = 46.0 - 8.0 * (age / seconds)
        phase = 2.0 * math.pi * slide * age
        beat = 0.6 + 0.4 * math.sin(2.0 * math.pi * 3.1 * age)
        rasp = random.uniform(-1.0, 1.0) * 0.06
        out.append((math.sin(phase) * 0.55 * beat + math.sin(phase * 1.51) * 0.18 + rasp) *
                   envelope(i, total, 0.7, 1.1))
    return out


CLIPS = {
    "water": water,
    "piano": piano,
    "footsteps": footsteps,
    "drone": drone,
}


def main():
    parser = argparse.ArgumentParser(description="Write the placeholder clips into sounds/")
    parser.add_argument("--out", default="sounds", help="where the clips go (default: sounds)")
    parser.add_argument("--force", action="store_true", help="rebuild clips that already exist")
    args = parser.parse_args()

    random.seed(7)
    os.makedirs(args.out, exist_ok=True)

    for name, build in sorted(CLIPS.items()):
        path = os.path.join(args.out, name + ".wav")
        if os.path.exists(path) and not args.force:
            print("[make_sounds] keeping {}".format(path))
            continue
        write(path, build())
        print("[make_sounds] wrote {}".format(path))


if __name__ == "__main__":
    main()
