#version 450

// This is a vertex attribute that it is read from a vertexBuffer
layout(location = 0) in vec3 position; 
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;

layout (set = 0, binding = 0) uniform GlobalUbo {
  mat4 projectionViewMatrix;
  vec3 directionToLight;
} ubo;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix; // keep as mat4 for alignment requirements, but truncate to mat3
} push;

// This is a SKY LIGHT, only direction, same for all vertices, no distance
const float AMBIENT = 0.02;

void main() {
  gl_Position = ubo.projectionViewMatrix * push.modelMatrix * vec4(position, 1.0);

  vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

  // default to 0 if light on other side
  float lightIntensity = AMBIENT + max(dot(normalWorldSpace, ubo.directionToLight), 0);

  fragColor = lightIntensity * color;
}