#version 450

// Anything lit and painted with a picture -- a floor, a wall, a ceiling
//
// Where the picture lands is worked out from world position, not from the mesh's
// own uv. Everything the map generates is a stretched cube whose uv is a box
// unwrap, which would smear one patch of the atlas over the whole surface

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 0) out vec4 outColor;

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

layout(set = 1, binding = 0) uniform sampler2D albedoMap;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  // A mat3 normal matrix, with the corners mat3() never reads carrying extras:
  // [3][0] [3][1] are how often the picture repeats, [3][2] is where the floor
  // is and [3][3] is the alpha
  mat4 normalMatrix;
} push;

// Which two axes the picture is laid out along, picked off the way the surface
// faces. A wall climbs from the floor, everything flat runs across the room
vec2 surfaceAt(vec3 world, vec3 normal, float ground) {
  vec3 facing = abs(normal);

  if (facing.y >= facing.x && facing.y >= facing.z) return world.xz;

  // -Y is up, so the height above the floor grows as y shrinks
  float climb = ground - world.y;
  return facing.z >= facing.x ? vec2(world.x, climb) : vec2(world.z, climb);
}

void main() {
  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorld);

  vec3 cameraPosWorld = ubo.invView[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

  for (int i = 0; i < ubo.numLights; i++) {
    PointLight light = ubo.pointLights[i];

    vec3 directionToLight = light.position.xyz - fragPosWorld;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngleToIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = light.color.xyz * light.color.w * attenuation;

    diffuseLight += intensity * cosAngleToIncidence;

    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(surfaceNormal, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1);

    // Boards are duller than the default surface
    blinnTerm = pow(blinnTerm, 64.0);

    specularLight += intensity * blinnTerm;
  }

  vec2 tile = vec2(push.normalMatrix[3][0], push.normalMatrix[3][1]);
  float ground = push.normalMatrix[3][2];
  vec2 surface = surfaceAt(fragPosWorld, surfaceNormal, ground);

  vec3 albedo = texture(albedoMap, surface * tile).rgb * fragColor;

  float alpha = push.normalMatrix[3][3];

  outColor = vec4(diffuseLight * albedo + specularLight * albedo * 0.25, alpha);
}
