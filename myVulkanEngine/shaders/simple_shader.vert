#version 450

// This is a vertex attribute that it is read from a vertexBuffer
layout(location = 0) in vec3 position; 
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform Push {
  mat4 transform;   // projection * view * model
  mat4 modelMatrix;
} push;

// This is a SKY LIGHT, only direction, same for all vertices, no distance
const vec3 DIRECTION_TO_LIGHT = normalize(vec3(1.0,-3.0,-1.0));
const float AMBIENT = 0.02;

void main() {
  gl_Position = push.transform * vec4(position, 1.0);

  // We transform to mat3 because this is a VECTOR calculation, dont need barycentric position values
  // vec3 normalWorldSpace = normalize(mat3(push.modelMatrix) * normal);

  // This is expensive!! it scales correctly but expensive
  mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));
  vec3 normalWorldSpace = normalize(normalMatrix * normal);

  // default to 0 if light on other side
  float lightIntensity = AMBIENT + max(dot(normalWorldSpace, DIRECTION_TO_LIGHT), 0);

  fragColor = lightIntensity * color;
}