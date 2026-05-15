#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

// This is how we receive our PUSH DATA
layout(push_constant) uniform Push {
  mat4 transform;
  vec3 color;
} push;

void main() {
  outColor = vec4(fragColor, 1.0);
  //outColor = vec4(push.color, 1.0);
}