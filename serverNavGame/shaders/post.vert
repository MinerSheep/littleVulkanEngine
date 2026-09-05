#version 450

// One triangle big enough to cover the screen, worked out from the vertex index
// so there is no vertex buffer and nothing to bind
layout(location = 0) out vec2 fragUv;

void main() {
  fragUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(fragUv * 2.0 - 1.0, 0.0, 1.0);
}
