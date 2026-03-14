#version 450


// pass in values using a vertex buffer instead of hard coding
vec2 positions[3] = vec2[] (
    vec2(0.0,-0.5),
    vec2(0.5,0.5),
    vec2(-0.5,0.5)
);

void main()
{
    // center is at 0,0 - top left (-1,-1) - bottom right (1,1)
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}