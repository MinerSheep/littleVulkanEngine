import math

bgBars, bgBarFill, bgSlant, bgSpeed = 14, 0.45, 0.35, 0.18

def bars(clock):
    slot = 2.0 / bgBars
    slide = math.fmod(clock * bgSpeed, slot)
    lead = 1 + int(math.ceil(abs(bgSlant) / slot))
    return slot, slide, lead, [(-1.0 + slide + i * slot) for i in range(-lead, bgBars + 2)]

def inked(ndc_x, ndc_y, clock):
    slot, slide, lead, xs = bars(clock)
    qy = (ndc_y + 1.0) / 2.0
    for x0 in xs:
        lo = x0 + qy * bgSlant
        if lo <= ndc_x <= lo + slot * bgBarFill:
            return True
    return False

# 1. coverage: does the bar pattern run past BOTH screen edges at EVERY row?
worst = None
for step in range(400):
    clock = step * 0.05
    slot, slide, lead, xs = bars(clock)
    for r in range(41):
        qy = r / 40.0
        left  = xs[0]  + qy * bgSlant                      # leftmost bar's left edge
        right = xs[-1] + qy * bgSlant + slot * bgBarFill   # rightmost bar's right edge
        m = min(-1.0 - left, right - 1.0)                  # margin past each edge
        if worst is None or m < worst[0]:
            worst = (m, clock, qy)
print("worst edge margin over 400 clocks x 41 rows: {:.4f} NDC (clock={:.2f}, qy={:.2f})".format(*worst))
print("  -> {}".format("OK, pattern always overruns both edges" if worst[0] > 0 else "BARE WEDGE, lead too small"))

# 2. seam: pattern at clock t must equal pattern one full slot later
slot = 2.0 / bgBars
period = slot / bgSpeed
diffs = 0
for r in range(21):
    for c in range(81):
        x, y = -1.0 + c / 40.0, -1.0 + r / 10.0
        if inked(x, y, 3.0) != inked(x, y, 3.0 + period):
            diffs += 1
print("seam check: {} of 1701 sample points differ across one wrap period -> {}".format(
    diffs, "SEAMLESS" if diffs == 0 else "SEAM"))

# 3. what it looks like
print("\npreview (# = bar, . = wash), 78x20 at clock=3.0:")
for r in range(20):
    y = -1.0 + 2.0 * r / 19.0
    print("  " + "".join("#" if inked(-1.0 + 2.0 * c / 77.0, y, 3.0) else "." for c in range(78)))
slot, slide, lead, xs = bars(3.0)
print("\nquads per frame: 1 wash + {} bars = {}".format(len(xs), len(xs) + 1))
