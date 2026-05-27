#version 450

// This is a vertex attribute that it is read from a vertexBuffer
layout(location = 0) in vec3 position; 
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;

layout (set = 0, binding = 0) uniform GlobalUbo {
  mat4 projectionViewMatrix;
  vec4 ambientLightColor;
  vec4 lightPosition;
  vec4 lightColor;
} ubo;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix; // keep as mat4 for alignment requirements, but truncate to mat3
} push;

void main() {
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position = ubo.projectionViewMatrix * positionWorld;

  vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

  // xyz to convert to vec3
  vec3 directionToLight = ubo.lightPosition.xyz - positionWorld.xyz;
  float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared

  vec3 lightColor = ubo.lightColor.xyz * ubo.lightColor.w * attenuation;
  vec3 ambientLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;

  // diffuse lighting is changed for colored lighting
  vec3 diffuseLight = lightColor * max(dot(normalWorldSpace, normalize(directionToLight)), 0);

  fragColor = (diffuseLight + ambientLight) * color;
}