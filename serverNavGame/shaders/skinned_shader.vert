#version 450

// Skinned vertex layout (see LveSkinnedModel::Vertex).
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in uvec4 joints;   // indices into the bone palette
layout(location = 4) in vec4 weights;   // blend weights (sum to 1)

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;

struct PointLight {
  vec4 position; // w is ignored
  vec4 color;    // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColor; // w is intensity
  PointLight pointLights[10];
  int numLights;
} ubo;

// Bone matrix palette for this model. std430 so each mat4 is tightly packed.
layout(std430, set = 1, binding = 0) readonly buffer JointBuffer {
  mat4 jointMatrices[];
};

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix; // keep as mat4 for alignment; truncated to mat3 below
} push;

void main() {
  // Linear blend skinning: weighted sum of the referenced joint matrices.
  mat4 skinMatrix =
      weights.x * jointMatrices[joints.x] +
      weights.y * jointMatrices[joints.y] +
      weights.z * jointMatrices[joints.z] +
      weights.w * jointMatrices[joints.w];

  vec4 positionWorld = push.modelMatrix * skinMatrix * vec4(position, 1.0);
  gl_Position = ubo.projection * ubo.view * positionWorld;

  fragPosWorld = positionWorld.xyz;
  mat3 skinNormal = mat3(push.normalMatrix) * mat3(skinMatrix);
  fragNormalWorld = normalize(skinNormal * normal);

  // Flat tint for the whole mesh, carried in the column mat3() throws away
  fragColor = push.normalMatrix[3].xyz;
}
