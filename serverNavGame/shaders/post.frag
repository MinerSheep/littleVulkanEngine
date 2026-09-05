#version 450

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D picture;

layout(push_constant) uniform Push {
  float wobble;    // how far a line slides sideways
  float separate;  // how far red and blue come apart
  float invert;    // how far the picture is turned inside out
  float vignette;  // how much the edges go
  float grain;     // how much dust is on it
  float drain;     // how much colour has left
  float time;      // what the sliding crawls on
} push;

// Enough randomness for dust and for a line to pick its own offset
float scatter(vec2 at) {
  return fract(sin(dot(at, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
  vec2 uv = fragUv;

  // Tape: whole lines slide, and the ones next to each other slide differently
  if (push.wobble > 0.0) {
    float line = floor(uv.y * 240.0);
    float slide = scatter(vec2(line, floor(push.time * 12.0))) - 0.5;
    uv.x += slide * push.wobble * 0.08;
  }

  vec3 lit = texture(picture, uv).rgb;

  // The colours come apart, red one way and blue the other
  if (push.separate > 0.0) {
    float apart = push.separate * 0.01;
    lit.r = texture(picture, uv + vec2(apart, 0.0)).r;
    lit.b = texture(picture, uv - vec2(apart, 0.0)).b;
  }

  if (push.drain > 0.0) {
    float flat_ = dot(lit, vec3(0.299, 0.587, 0.114));
    lit = mix(lit, vec3(flat_), push.drain);
  }

  if (push.invert > 0.0) lit = mix(lit, vec3(1.0) - lit, push.invert);

  if (push.grain > 0.0) {
    float dust = scatter(uv * vec2(640.0, 480.0) + push.time) - 0.5;
    lit += dust * push.grain * 0.35;
  }

  if (push.vignette > 0.0) {
    vec2 off = uv - 0.5;
    float edge = 1.0 - dot(off, off) * 2.4 * push.vignette;
    lit *= clamp(edge, 0.0, 1.0);
  }

  outColor = vec4(lit, 1.0);
}
