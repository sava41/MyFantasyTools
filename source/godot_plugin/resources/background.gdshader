shader_type spatial;

render_mode unshaded, fog_disabled;

uniform sampler2D color_direct;
uniform sampler2D color_indirect;
uniform sampler2D depth;
uniform sampler2D depth_buffer : hint_depth_texture;

uniform float fov = 1.0;
uniform float uncropped_fov = 1.0;
uniform float uncropped_aspect = 1.0;
uniform mat4 uncropped_view_mat = mat4(1.0);

// Raymarching parameters
uniform int ray_steps : hint_range(1, 64) = 16;
uniform float ray_max_distance = 3;
uniform float ray_thickness = 0.1;

const uint sectorCount = 32u;
uniform int sampleCount = 3;
uniform float sampleRadius = 0.15;
uniform int sliceCount = 4;
uniform float hitThickness = 0.25;
uniform float sampleOffset = 0.01;

const float pi = 3.14159265359;
const float twoPi = 2.0 * pi;
const float halfPi = 0.5 * pi;

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float randf(int x, int y) {
    return mod(52.9829189 * mod(0.06711056 * float(x) + 0.00583715 * float(y), 1.0), 1.0);
}

// https://graphics.stanford.edu/%7Eseander/bithacks.html
uint bit_count(uint value) {
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

// https://cdrinmatane.github.io/posts/ssaovb-code/
uint update_sectors(float minHorizon, float maxHorizon, uint outBitfield) {
    uint startBit = uint(minHorizon * float(sectorCount));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(sectorCount)));
    uint angleBit = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (sectorCount - horizonAngle)) : 0u;
    uint currentBitfield = angleBit << startBit;
    return outBitfield | currentBitfield;
}

vec4 sample_zero_outside(sampler2D tex, vec2 uv) {
	return (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
		? texture(tex, uv)
		: vec4(0.0);
}

vec2 fast_acos2(vec2 x) {
	return (-0.69813170*x*x - 0.87266463)*x + 1.57079633;
}

float fast_acos(float x) {
	return (-0.69813170*x*x - 0.87266463)*x + 1.57079633;
}

// AO algo based on GTVBAO
float get_ao(vec2 screen_uv, vec2 aspect, vec3 view_point, vec3 view_normal, mat4 proj, mat4 inv_proj, float jitter) {

    vec3 view_dir = normalize(-view_point);

	float slice_angle_step = pi / float(sliceCount);
    float sample_scale = (-sampleRadius * proj[0][0]) / view_point.z;

	float ao_acc = 0.0;
	float weight_acc = 0.0;
	float phi = jitter;

	for (int i = 0; i < sliceCount; i++) {

        vec2 omega = vec2(cos(phi), sin(phi));
        vec3 direction = vec3(-omega.x, omega.y, 0.0);
        vec3 slice_normal = cross(direction, view_dir);
        vec3 projNormal = view_normal - slice_normal * dot(view_normal, slice_normal);
        float projLength = length(projNormal);
		float cosN = dot(projNormal, view_dir) / projLength;

		vec3 orthoDirection = cross(view_dir, slice_normal);
        float signN = sign(dot(projNormal, orthoDirection));
        float n = signN * fast_acos(cosN);

		vec2 uv_sample_step = 1.0 / float(sampleCount) * sample_scale * omega * aspect;
		vec2 uv_sample_offset =  (abs(jitter) / float(sampleCount) + sampleOffset) * sample_scale * omega * aspect;

		uint sector_bitmask = 0u;
		for (int i = 0; i < sampleCount; i++) {
			for(float d = -1.0; d <= 1.0; d += 2.0)
        	{
				vec2 sampleUV = clamp(screen_uv + uv_sample_offset * d, vec2(0.0), vec2(1.0));

				float depth_sample = texture(depth_buffer, sampleUV).x;

				vec4 sample_ndc = vec4(sampleUV * 2.0 - 1.0, depth_sample, 1.0);
				vec4 sample_view = inv_proj * sample_ndc;
				sample_view.xyz /= sample_view.w;

				vec3 delta_pos_front = sample_view.xyz - view_point;
				vec3 delta_pos_back = delta_pos_front - view_dir * hitThickness;

				vec2 frontBackHorizon = vec2(dot(normalize(delta_pos_front), view_dir), dot(normalize(delta_pos_back), view_dir));

				frontBackHorizon = acos(frontBackHorizon);

				frontBackHorizon = clamp((d * frontBackHorizon + n + halfPi) / pi, 0.0, 1.0);

				frontBackHorizon = d >= 0.0 ? frontBackHorizon.xy : frontBackHorizon.yx;

				sector_bitmask = update_sectors(frontBackHorizon.x, frontBackHorizon.y, sector_bitmask);
			}

			uv_sample_offset += uv_sample_step;
		}

		// Weigh slices based on how far their projected normal is from the surface normal
		ao_acc += projLength * (1.0 - float(bit_count(sector_bitmask)) / float(sectorCount));
		weight_acc += projLength;

		phi += slice_angle_step;
    }

    return ao_acc / weight_acc;
}

void vertex() {
	// Transform chain: ndc_uncropped -> view_uncropped -> world -> view_cropped -> ndc_cropped
	vec2 ndc_uncropped = VERTEX.xy;

	float tan_half_fov_h_uncropped = tan(uncropped_fov * 0.5);
	float tan_half_fov_v_uncropped = tan_half_fov_h_uncropped * uncropped_aspect;

	vec4 view_dir_uncropped = vec4(
		ndc_uncropped.x * tan_half_fov_h_uncropped,
		ndc_uncropped.y * tan_half_fov_v_uncropped,
		-1.0,
		1.0
	);

	vec4 ndc_cropped = PROJECTION_MATRIX * VIEW_MATRIX * uncropped_view_mat * view_dir_uncropped;

	POSITION = ndc_cropped;
	COLOR = view_dir_uncropped;
}

void fragment() {

	float linear_depth_uncropped = texture(depth, UV).x;
	vec4 view_dir_uncropped = COLOR;

	vec3 uncropped_view_depth_pos = -view_dir_uncropped.xyz * (linear_depth_uncropped / view_dir_uncropped.z);

	vec4 world_depth_point = uncropped_view_mat * vec4(uncropped_view_depth_pos, 1.0);
	world_depth_point.xyz /= world_depth_point.w;
	world_depth_point.w = 1.0;

	vec4 view_depth_pos = VIEW_MATRIX * world_depth_point;
	view_depth_pos.xyz /= view_depth_pos.w;
	view_depth_pos.w = 1.0;

	vec4 clip_pos = PROJECTION_MATRIX * view_depth_pos;
	clip_pos.xyz /= clip_pos.w;
	DEPTH = clip_pos.z;

	//we need to comp the depth buffer and the depth texture before we calculate ao
	float buffer_depth = texture(depth_buffer, SCREEN_UV).x;

	if(buffer_depth > clip_pos.z)
	{
		vec4 buffer_ndc = vec4(SCREEN_UV * 2.0 - 1.0, buffer_depth, 1.0);
		vec4 buffer_view = INV_PROJECTION_MATRIX * buffer_ndc;
		buffer_view.xyz /= buffer_view.w;

		view_depth_pos.xyz = buffer_view.xyz;
	}

	vec3 view_point_ddx = dFdx(view_depth_pos.xyz);
	vec3 view_point_ddy = dFdy(view_depth_pos.xyz);
	vec3 view_point_normal = normalize(cross(view_point_ddx, -view_point_ddy));

	vec2 aspect = VIEWPORT_SIZE.yx / VIEWPORT_SIZE.x;
	float jitter = randf(int(FRAGCOORD.x), int(FRAGCOORD.y)) - 0.5;
	float ao = get_ao(SCREEN_UV, aspect, view_depth_pos.xyz, view_point_normal, PROJECTION_MATRIX, INV_PROJECTION_MATRIX, jitter);

	vec3 sampled_color_direct = texture(color_direct, UV).rgb;
	vec3 sampled_color_indirect = texture(color_indirect, UV).rgb;

	ALBEDO.rgb = sampled_color_direct + sampled_color_indirect * ao;
}
