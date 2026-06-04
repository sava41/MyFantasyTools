#[vertex]

#version 450

layout(location = 0) in vec2 vertex_pos;  // NDC [-1, 1]

void main() {
    gl_Position = vec4(vertex_pos, 0.0, 1.0);
}

#[fragment]

#version 450

layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform sampler2D beauty_tex;
layout(set = 1, binding = 1) uniform sampler2D bg_depth_tex;

layout(set = 0, binding = 0, std140) uniform Params {
    mat4 inv_projection;
    mat4 projection;
    mat4 view_matrix;
    mat4 inv_view_matrix;
    mat4 uncropped_view_mat;
    mat4 inv_uncropped_view_mat;
    float uncropped_fov;
    float uncropped_aspect;
    int screen_w;
    int screen_h;
} params;

void main() {
    vec2 screen_uv = gl_FragCoord.xy / vec2(params.screen_w, params.screen_h);
    frag_color   = texture(beauty_tex,   screen_uv);
    gl_FragDepth = texture(bg_depth_tex, screen_uv).r;
}
