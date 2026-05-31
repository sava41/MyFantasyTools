#[compute]

#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Set 0 — color buffer output (written as storage image)
layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D color_image;

// Set 1 — baked textures + scene depth
layout(set = 1, binding = 0) uniform sampler2D color_direct_tex;
layout(set = 1, binding = 1) uniform sampler2D color_indirect_tex;
layout(set = 1, binding = 2) uniform sampler2D depth_baked_tex;
layout(set = 1, binding = 3) uniform sampler2D scene_depth_tex;

// Set 2 — per-frame params UBO (std140)
layout(set = 2, binding = 0, std140) uniform Params {
    mat4 inv_projection;
    mat4 projection;
    mat4 view_matrix;            // world → view (VIEW_MATRIX)
    mat4 inv_view_matrix;        // view → world (camera world transform)
    mat4 uncropped_view_mat;     // uncropped camera → world
    mat4 inv_uncropped_view_mat; // world → uncropped camera
    float uncropped_fov;
    float uncropped_aspect;
    int screen_w;
    int screen_h;
} params;

// ---- GTAO constants (match background.gdshader defaults) ----
const uint  SECTOR_COUNT  = 32u;
const int   SAMPLE_COUNT  = 3;
const float SAMPLE_RADIUS = 0.15;
const int   SLICE_COUNT   = 4;
const float HIT_THICKNESS = 0.25;
const float SAMPLE_OFFSET = 0.01;
const float PI            = 3.14159265359;

// ---- Helpers ported from background.gdshader ----

float randf(int x, int y) {
    return mod(52.9829189 * mod(0.06711056 * float(x) + 0.00583715 * float(y), 1.0), 1.0);
}

uint bit_count(uint value) {
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

uint update_sectors(float minHorizon, float maxHorizon, uint outBitfield) {
    uint startBit    = uint(minHorizon * float(SECTOR_COUNT));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(SECTOR_COUNT)));
    uint angleBit    = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (SECTOR_COUNT - horizonAngle)) : 0u;
    return outBitfield | (angleBit << startBit);
}

float fast_acos(float x) {
    return (-0.69813170*x*x - 0.87266463)*x + 1.57079633;
}

float get_ao(vec2 screen_uv, vec2 aspect, vec3 view_point, vec3 view_normal, float jitter) {
    vec3  view_dir         = normalize(-view_point);
    float slice_angle_step = PI / float(SLICE_COUNT);
    float sample_scale     = (-SAMPLE_RADIUS * params.projection[0][0]) / view_point.z;

    float ao_acc = 0.0, weight_acc = 0.0;
    float phi = jitter;

    for (int i = 0; i < SLICE_COUNT; i++) {
        vec2 omega     = vec2(cos(phi), sin(phi));
        vec3 direction = vec3(-omega.x, omega.y, 0.0);
        vec3 sliceN    = cross(direction, view_dir);
        vec3 projN     = view_normal - sliceN * dot(view_normal, sliceN);
        float projLen  = length(projN);
        float cosN     = dot(projN, view_dir) / projLen;
        float signN    = sign(dot(projN, cross(view_dir, sliceN)));
        float n        = signN * fast_acos(cosN);

        vec2 uv_step   = (1.0 / float(SAMPLE_COUNT)) * sample_scale * omega * aspect;
        vec2 uv_off    = (abs(jitter) / float(SAMPLE_COUNT) + SAMPLE_OFFSET) * sample_scale * omega * aspect;

        uint bitmask = 0u;
        for (int j = 0; j < SAMPLE_COUNT; j++) {
            for (float d = -1.0; d <= 1.0; d += 2.0) {
                vec2  sUV  = clamp(screen_uv + uv_off * d, vec2(0.0), vec2(1.0));
                float sDepth = texture(scene_depth_tex, sUV).x;

                vec4 sNDC  = vec4(sUV * 2.0 - 1.0, sDepth, 1.0);
                vec4 sView = params.inv_projection * sNDC;
                sView.xyz /= sView.w;

                vec3 df = sView.xyz - view_point;
                vec3 db = df - view_dir * HIT_THICKNESS;

                vec2 fbh = vec2(dot(normalize(df), view_dir), dot(normalize(db), view_dir));
                fbh = acos(fbh);
                fbh = clamp((d * fbh + n + PI * 0.5) / PI, 0.0, 1.0);
                fbh = d >= 0.0 ? fbh.xy : fbh.yx;

                bitmask = update_sectors(fbh.x, fbh.y, bitmask);
            }
            uv_off += uv_step;
        }

        ao_acc     += projLen * (1.0 - float(bit_count(bitmask)) / float(SECTOR_COUNT));
        weight_acc += projLen;
        phi        += slice_angle_step;
    }

    return ao_acc / max(weight_acc, 1e-5);
}

// ---- Main ----

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= params.screen_w || coord.y >= params.screen_h) return;

    vec2 screen_size = vec2(params.screen_w, params.screen_h);
    vec2 screen_uv   = (vec2(coord) + 0.5) / screen_size;
    vec2 ndc         = screen_uv * 2.0 - 1.0;  // Vulkan NDC (y=-1 top, y=+1 bottom)

    // ---- Compute uncropped UV ----
    // Unproject screen pixel to view-space direction
    vec4 view_h = params.inv_projection * vec4(ndc, 1.0, 1.0);
    view_h.xyz /= view_h.w;

    // view → world → uncropped camera
    vec3 world_dir       = (params.inv_view_matrix   * vec4(view_h.xyz, 0.0)).xyz;
    vec4 uncropped_h     =  params.inv_uncropped_view_mat * vec4(world_dir, 0.0);

    // Scale so z = -1 (matching the original view_dir_uncropped convention)
    if (abs(uncropped_h.z) < 1e-6) return;
    vec3 uncropped_dir = uncropped_h.xyz / (-uncropped_h.z);  // z becomes -1

    float tan_h = tan(params.uncropped_fov * 0.5);
    float tan_v = tan_h * params.uncropped_aspect;

    // ndc_uncropped is the UV-space position on the uncropped image
    vec2 ndc_uncropped = vec2(uncropped_dir.x / tan_h, uncropped_dir.y / tan_v);
    vec2 uv            = vec2( ndc_uncropped.x * 0.5 + 0.5, 0.5 - ndc_uncropped.y * 0.5 );

    // Discard if this screen pixel is outside the uncropped image
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return;

    // ---- Reconstruct background world position from baked depth ----
    float linear_depth = texture(depth_baked_tex, uv).x;
    // view_dir_uncropped.z == -1, so position = dir * linear_depth
    vec3 uncropped_view_pos = vec3(uncropped_dir.x * linear_depth,
                                   uncropped_dir.y * linear_depth,
                                   -linear_depth);

    vec4 world_depth_pt = params.uncropped_view_mat * vec4(uncropped_view_pos, 1.0);
    world_depth_pt.xyz /= world_depth_pt.w;

    vec4 view_depth_pos = params.view_matrix * vec4(world_depth_pt.xyz, 1.0);
    view_depth_pos.xyz /= view_depth_pos.w;

    // ---- Background clip-space Z (for depth comparison) ----
    vec4 clip_pos = params.projection * view_depth_pos;
    float background_depth = clip_pos.z / clip_pos.w;

    // ---- Scene depth test ----
    float scene_d = texture(scene_depth_tex, screen_uv).x;

    // In reverse-Z: larger = closer. Background visible where scene_d <= background_depth.
    if (scene_d > background_depth + 1e-4) return;

    // ---- If scene geometry is closer, use it for GTAO position ----
    if (scene_d > background_depth) {
        vec4 buf_ndc  = vec4(screen_uv * 2.0 - 1.0, scene_d, 1.0);
        vec4 buf_view = params.inv_projection * buf_ndc;
        view_depth_pos.xyz = buf_view.xyz / buf_view.w;
    }

    // ---- Surface normal estimate from scene depth derivatives ----
    vec2 texel = 1.0 / screen_size;

    float d_r  = texture(scene_depth_tex, screen_uv + vec2(texel.x, 0.0)).x;
    float d_u  = texture(scene_depth_tex, screen_uv + vec2(0.0, texel.y)).x;

    vec4 vr = params.inv_projection * vec4((screen_uv + vec2(texel.x, 0.0)) * 2.0 - 1.0, d_r, 1.0);
    vec4 vu = params.inv_projection * vec4((screen_uv + vec2(0.0, texel.y)) * 2.0 - 1.0, d_u, 1.0);
    vr.xyz /= vr.w;
    vu.xyz /= vu.w;

    vec3 ddx    = vr.xyz - view_depth_pos.xyz;
    vec3 ddy    = vu.xyz - view_depth_pos.xyz;
    vec3 normal = normalize(cross(ddx, -ddy));

    // ---- GTAO ----
    vec2  aspect = screen_size.yx / screen_size.x;
    float jitter = randf(coord.x, coord.y) - 0.5;
    float ao     = get_ao(screen_uv, aspect, view_depth_pos.xyz, normal, jitter);

    // ---- Composite ----
    vec3 direct   = texture(color_direct_tex,   uv).rgb;
    vec3 indirect = texture(color_indirect_tex, uv).rgb;

    imageStore(color_image, coord, vec4(direct + indirect * ao, 1.0));
}
