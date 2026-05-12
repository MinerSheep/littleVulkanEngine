#version 450

// This is a vertex attribute that it is read from a vertexBuffer
layout(location = 0) in vec2 position; 
layout(location = 1) in vec3 color;

//layout(location = 0) out vec3 fragColor;

// This is how we receive our PUSH DATA
layout(push_constant) uniform Push {
  vec2 offset;
  vec3 color;
} push;

void main() {
  gl_Position = vec4(position + push.offset, 0.0, 1.0);

  //fragColor = color;
}