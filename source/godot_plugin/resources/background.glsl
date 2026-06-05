#[vertex]

#version 450

layout(location = 0) in vec2 vertex_pos;  // uncropped NDC [-1, 1]

layout(location = 0) out vec3 out_view_dir;  // uncropped view direction (z = -1)
layout(location = 1) out vec2 out_uv;        // uncropped texture UV

layout(set = 0, binding = 0, std140) uniform Params {
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

void main() {
    float tan_h = tan(params.uncropped_fov * 0.5);
    float tan_v = tan_h * params.uncropped_aspect;

    // Point in uncropped view space at z = -1
    vec4 view_dir = vec4(vertex_pos.x * tan_h, vertex_pos.y * tan_v, -1.0, 1.0);

    // Transform chain: uncropped view → world → cropped view → clip
    gl_Position = params.projection * params.view_matrix * params.uncropped_view_mat * view_dir;

    out_view_dir = view_dir.xyz;
    out_uv       = vec2(vertex_pos.x * 0.5 + 0.5, 0.5 - vertex_pos.y * 0.5);
}


#[fragment]

#version 450

layout(location = 0) in vec3 in_view_dir;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4  frag_beauty;    // direct + indirect * ao
layout(location = 1) out vec4  frag_ssao;      // ao in .r
layout(location = 2) out float frag_bg_depth;  // background clip-space depth (z/w)

layout(set = 1, binding = 0) uniform sampler2D direct_diffuse_tex;
layout(set = 1, binding = 1) uniform sampler2D direct_specular_tex;
layout(set = 1, binding = 2) uniform sampler2D indirect_diffuse_tex;
layout(set = 1, binding = 3) uniform sampler2D indirect_specular_tex;
layout(set = 1, binding = 4) uniform sampler2D depth_baked_tex;
layout(set = 1, binding = 5) uniform sampler2D normal_baked_tex;
layout(set = 1, binding = 6) uniform sampler2D depth_scene_tex;
layout(set = 1, binding = 7) uniform sampler2D scene_color_tex;

layout(set = 0, binding = 0, std140) uniform Params {
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

// ---- GTAO constants ----
const uint  SECTOR_COUNT  = 32u;
const int   SAMPLE_COUNT  = 3;
const float SAMPLE_RADIUS = 0.15;
const int   SLICE_COUNT   = 4;
const float HIT_THICKNESS = 0.25;
const float SAMPLE_OFFSET = 0.01;
const float PI            = 3.14159265359;

// ---- Helpers ----

float randf(int x, int y) {
    return mod(52.9829189 * mod(0.06711056 * float(x) + 0.00583715 * float(y), 1.0), 1.0);
}

uint bit_count(uint value) {
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

uint update_sectors(float minHorizon, float maxHorizon, uint outBitfield) {
    uint startBit     = uint(minHorizon * float(SECTOR_COUNT));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(SECTOR_COUNT)));
    uint angleBit     = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (SECTOR_COUNT - horizonAngle)) : 0u;
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

        vec2 uv_step = (1.0 / float(SAMPLE_COUNT)) * sample_scale * omega * aspect;
        vec2 uv_off  = (abs(jitter) / float(SAMPLE_COUNT) + SAMPLE_OFFSET) * sample_scale * omega * aspect;

        uint bitmask = 0u;
        for (int j = 0; j < SAMPLE_COUNT; j++) {
            for (float d = -1.0; d <= 1.0; d += 2.0) {
                vec2  sUV    = clamp(screen_uv + uv_off * d, vec2(0.0), vec2(1.0));
                float sDepth = texture(depth_scene_tex, sUV).x;

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

// ---- SSR ----

const int   SSR_STEPS       = 16;
const float SSR_STEP_SIZE   = 0.05;
const float SSR_SPEC_CUTOFF = 0.01;

bool trace_ssr(vec3 view_pos, vec3 view_normal, out vec2 hit_uv) {
    vec3 view_dir    = normalize(-view_pos);
    vec3 reflect_dir = reflect(-view_dir, view_normal);
    vec3 ray_pos     = view_pos + reflect_dir * SSR_STEP_SIZE;

    for (int i = 0; i < SSR_STEPS; i++) {
        vec4 clip      = params.projection * vec4(ray_pos, 1.0);
        vec2 screen_uv = clip.xy / clip.w * 0.5 + 0.5;

        if (any(lessThan(screen_uv, vec2(0.0))) || any(greaterThan(screen_uv, vec2(1.0))))
            break;

        float scene_d = texture(depth_scene_tex, screen_uv).x;
        float ray_d   = clip.z / clip.w;   // reverse-Z

        if (scene_d > 1e-4 && scene_d > ray_d - 0.02) {
            hit_uv = screen_uv;
            return true;
        }
        ray_pos += reflect_dir * SSR_STEP_SIZE;
    }
    return false;
}

// ---- Main ----

void main() {
    vec2 screen_size = vec2(params.screen_w, params.screen_h);
    vec2 screen_uv   = gl_FragCoord.xy / screen_size;

    // ---- Reconstruct background world position from baked depth ----
    float linear_depth       = texture(depth_baked_tex, in_uv).x;
    vec3  uncropped_view_pos = in_view_dir * linear_depth;

    vec4 world_depth_pt = params.uncropped_view_mat * vec4(uncropped_view_pos, 1.0);
    world_depth_pt.xyz /= world_depth_pt.w;

    vec4 view_depth_pos = params.view_matrix * vec4(world_depth_pt.xyz, 1.0);
    view_depth_pos.xyz /= view_depth_pos.w;

    // ---- Background clip-space depth ----
    vec4  clip_pos        = params.projection * view_depth_pos;
    float background_depth = clip_pos.z / clip_pos.w;

    // ---- Scene depth test (discard where scene geometry is closer) ----
    float scene_d = texture(depth_scene_tex, screen_uv).x;
    if (scene_d > background_depth + 1e-4) discard;

    // ---- If scene geometry is at the same depth, use it for GTAO position ----
    if (scene_d > background_depth) {
        vec4 buf_ndc  = vec4(screen_uv * 2.0 - 1.0, scene_d, 1.0);
        vec4 buf_view = params.inv_projection * buf_ndc;
        view_depth_pos.xyz = buf_view.xyz / buf_view.w;
    }

    // ---- Surface normal estimate from scene depth derivatives ----
    vec2 texel = 1.0 / screen_size;

    float d_r = texture(depth_scene_tex, screen_uv + vec2(texel.x, 0.0)).x;
    float d_u = texture(depth_scene_tex, screen_uv + vec2(0.0, texel.y)).x;

    vec4 vr = params.inv_projection * vec4((screen_uv + vec2(texel.x, 0.0)) * 2.0 - 1.0, d_r, 1.0);
    vec4 vu = params.inv_projection * vec4((screen_uv + vec2(0.0, texel.y)) * 2.0 - 1.0, d_u, 1.0);
    vr.xyz /= vr.w;
    vu.xyz /= vu.w;

    vec3 ddx    = vr.xyz - view_depth_pos.xyz;
    vec3 ddy    = vu.xyz - view_depth_pos.xyz;
    vec3 normal = normalize(cross(ddx, -ddy));

    // ---- GTAO ----
    vec2  aspect = screen_size.yx / screen_size.x;
    float jitter = randf(int(gl_FragCoord.x), int(gl_FragCoord.y)) - 0.5;
    float ao     = get_ao(screen_uv, aspect, view_depth_pos.xyz, normal, jitter);

    // ---- Baked normal (camera-space XY, reconstruct Z assuming front-facing) ----
    vec2 n_xy = texture(normal_baked_tex, in_uv).xy;
    vec3 baked_normal = normalize(vec3(n_xy, sqrt(max(0.0, 1.0 - dot(n_xy, n_xy)))));

    // ---- Per-channel lighting ----
    vec3 direct_diffuse    = texture(direct_diffuse_tex,    in_uv).rgb;
    vec3 direct_specular   = texture(direct_specular_tex,   in_uv).rgb;
    vec3 indirect_diffuse  = texture(indirect_diffuse_tex,  in_uv).rgb;
    vec3 indirect_specular = texture(indirect_specular_tex, in_uv).rgb;

    // ---- SSR: replace specular with reflected scene geometry color on hit ----
    vec3 specular = direct_specular + indirect_specular;
    float spec_lum = dot(specular, vec3(0.2126, 0.7152, 0.0722));
    if (spec_lum > SSR_SPEC_CUTOFF) {
        vec2 ssr_uv;
        if (trace_ssr(view_depth_pos.xyz, baked_normal, ssr_uv))
            specular = texture(scene_color_tex, ssr_uv).rgb;
    }

    frag_beauty   = vec4(direct_diffuse + indirect_diffuse * ao + specular, 1.0);
    frag_ssao     = vec4(ao, 0.0, 0.0, 1.0);
    frag_bg_depth = background_depth;
}
