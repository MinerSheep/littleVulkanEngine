from pathlib import Path

# Resolve ../saves relative to this script's location
SAVES_DIR = Path(__file__).resolve().parent.parent / "saves"

FILES = {
    "forest.save": (
        b"save 1\n"
        b"room Start\n"
        b"played 1790613849\n"
        b"item @seconds 4\n"
        b"item @timeofday 2\n"
        b"item @visits.Start 1\n"
        b"prop Start.station  -3.000 -0.520 -5.000  0.000 0.540 0.000  0.160 0.110 0.220  hidden 0  flips 0\n"
    ),
    "progress.save": b"area forest\n",
}


def main():
    SAVES_DIR.mkdir(parents=True, exist_ok=True)
    for name, data in FILES.items():
        path = SAVES_DIR / name
        path.write_bytes(data)
        print(f"Wrote {path} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
