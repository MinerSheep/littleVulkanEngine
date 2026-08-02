#version 450

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

  // Here we can use SPECIALIZATION CONSTANTS - pass defines into shader
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  // We are full on 128 bytes, this is a mat3 normal, the [3][3] is used to represent the alpha
  mat4 normalMatrix;
} push;

void main() {
  // our constants for point lights
  vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 specularLight = vec3(0.0);
  vec3 surfaceNormal = normalize(fragNormalWorld);

  // last column + xyz for cam world position
  vec3 cameraPosWorld = ubo.invView[3].xyz;
  vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

  // here we loop through lights in ubo  (NOT USING PUSH)
  for (int i = 0; i < ubo.numLights; i++) {
    PointLight light = ubo.pointLights[i];

    // for each light, calculate diffuse light contribution to the total
    vec3 directionToLight = light.position.xyz - fragPosWorld;
    float attenuation = 1.0 / dot(directionToLight, directionToLight); // distance squared
    directionToLight = normalize(directionToLight);

    float cosAngleToIncidence = max(dot(surfaceNormal, directionToLight), 0);
    vec3 intensity = light.color.xyz * light.color.w * attenuation;

    diffuseLight += intensity * cosAngleToIncidence;

    // compute specular component
    vec3 halfAngle = normalize(directionToLight + viewDirection);
    float blinnTerm = dot(surfaceNormal, halfAngle);
    blinnTerm = clamp(blinnTerm, 0, 1); // ignore case when viewer is on opposite side of surface (negative values)

    // pass this exponent INTO SHADER based on OBJECT MATERIAL
    blinnTerm = pow(blinnTerm, 32.0);  // higher value -> sharper highlight

    specularLight += intensity * blinnTerm;
  }

  // How solid this object is. The blend is src.a * src + (1 - src.a) * dst, so the
  // colour stays as it is lit and only the last channel decides how much shows
  float alpha = push.normalMatrix[3][3];

  outColor = vec4(diffuseLight * fragColor + specularLight * fragColor, alpha);
}