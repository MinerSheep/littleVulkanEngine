#version 450

layout (location = 0) in vec2 fragOffset;
layout (location = 0) out vec4 outColor;

struct PointLight {
  vec4 position; // w is ignored
  vec4 color;    // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColor; // w is intensity

  // Here we can use SPECIALIZATION CONSTANTS - pass defines into shader
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push {
  vec4 position;
  vec4 color;
  float radius;
} push;

const float M_PI = 3.1415926;

void main() {
  float dis = sqrt(dot(fragOffset, fragOffset));
  if (dis >= 1.0) {
    discard;
  }

  // this creates a graph f(x) = 1/2 [cos(pi*x) + 1]

  // we are adjusting so that we start from 1.0 and go 0.0 further out
  float cosDis = 0.5 * (cos(dis * M_PI) + 1.0); // makes light white in center and fade out to color
  outColor = vec4(push.color.xyz + 0.5 * cosDis, cosDis);
}