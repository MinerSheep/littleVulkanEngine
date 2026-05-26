#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix; // keep as mat4 for alignment requirements, but truncate to mat3
} push;

void main() {
  outColor = vec4(fragColor, 1.0);
  //outColor = vec4(push.color, 1.0);
}