#version 450
#extension GL_ARB_separate_shader_objects : enable

// Full-screen quad positions
vec2 positions[6] = vec2[](
    vec2 (-1, 1), 
    vec2 (-1, -1), 
    vec2 (1, -1),

    vec2 (-1, 1), 
    vec2 (1, -1), 
    vec2 (1, 1)
  );

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
}
