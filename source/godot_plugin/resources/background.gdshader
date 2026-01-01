shader_type spatial;

render_mode unshaded, fog_disabled;

uniform sampler2D color_direct;
uniform sampler2D color_indirect;
uniform sampler2D depth;
uniform sampler2D light_direction;
uniform sampler2D depth_buffer : hint_depth_texture;

uniform float fov = 1.0;
uniform float uncropped_fov = 1.0;
uniform float uncropped_aspect = 1.0;
uniform mat4 uncropped_view_mat = mat4(1.0);

// Raymarching parameters
uniform int ray_steps : hint_range(1, 64) = 16;
uniform float ray_max_distance = 3;
uniform float ray_thickness = 0.1;

// Capsule shadow parameters
const int MAX_CAPSULES = 10;
uniform vec3 capsule_starts[MAX_CAPSULES];
uniform vec3 capsule_ends[MAX_CAPSULES];
uniform float capsule_radii[MAX_CAPSULES];
uniform int capsule_count = 0;
uniform float shadow_strength : hint_range(0.0, 2.0) = 1.5;
uniform float shadow_falloff : hint_range(0.1, 10.0) = 2.0;

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

// Decode octohedral encoded direction vector
// Input: encoded.rg = octohedral coords in [0,1], encoded.b = cone half angle
// Output: decoded world-space direction vector
// Also converts from Blender (Z-up) to Godot (Y-up) coordinate system
vec3 decode_octohedral_direction(vec3 encoded) {
	// Decode octohedral coordinates from [0,1] to [-1,1]
	vec2 oct = encoded.rg * 2.0 - 1.0;

	// Reconstruct the 3D direction from octohedral encoding
	vec3 n = vec3(oct.x, oct.y, 1.0 - abs(oct.x) - abs(oct.y));

	// Apply correction for negative z hemisphere
	if (n.z < 0.0) {
		vec2 old_xy = n.xy;
		n.x = (1.0 - abs(old_xy.y)) * sign(old_xy.x);
		n.y = (1.0 - abs(old_xy.x)) * sign(old_xy.y);
	}

	n = normalize(n);

	// Convert from Blender (Z-up) to Godot (Y-up) coordinate system
	// Blender: X right, Y forward, Z up
	// Godot:   X right, Y up, Z back
	return vec3(n.x, n.z, -n.y);
}

// Extract cone half angle from encoded data
float decode_cone_angle(vec3 encoded) {
	// Decode from [0,1] normalized range back to radians
	return encoded.b * halfPi;
}

// Fast inverse square root approximation
float fast_acos_positive(float x) {
	float p = -0.1565827 * x + 1.570796;
	return p * sqrt(1.0 - x);
}

float fast_acos_full(float x) {
	float y = abs(x);
	float p = -0.1565827 * y + 1.570796;
	p *= sqrt(1.0 - y);
	return x >= 0.0 ? p : pi - p;
}

// Spherical caps intersection for directional occlusion
// Based on "Ambient Aperture Lighting" by Oat and Sander 2007
float spherical_caps_intersection(float cos_cap1, float cos_cap2, float cap2, float cos_distance) {
	float r1 = fast_acos_positive(cos_cap1);
	float r2 = cap2;
	float d = fast_acos_full(cos_distance);

	if (min(r1, r2) <= max(r1, r2) - d) {
		return 1.0 - max(cos_cap1, cos_cap2);
	} else if (r1 + r2 <= d) {
		return 0.0;
	}

	float delta = abs(r1 - r2);
	float x = 1.0 - clamp((d - delta) / max(r1 + r2 - delta, 0.0001), 0.0, 1.0);
	float area = x * x * (-2.0 * x + 3.0); // simplified smoothstep
	return area * (1.0 - max(cos_cap1, cos_cap2));
}

// Directional occlusion from a sphere
float directional_occlusion_sphere(vec3 pos, vec3 sphere_center, float sphere_radius, vec3 light_dir, float cone_angle) {
	vec3 occluder = sphere_center - pos;
	float occluder_length2 = dot(occluder, occluder);
	vec3 occluder_dir = occluder * inversesqrt(occluder_length2);

	float cos_phi = dot(occluder_dir, light_dir);
	float cos_theta = sqrt(occluder_length2 / (sphere_radius * sphere_radius + occluder_length2));
	float cos_cone = cos(cone_angle);

	return 1.0 - spherical_caps_intersection(cos_theta, cos_cone, cone_angle, cos_phi) / (1.0 - cos_cone);
}

// Directional occlusion from a capsule
float directional_occlusion_capsule(vec3 pos, vec3 capsule_a, vec3 capsule_b, float capsule_radius, vec3 light_dir, float cone_angle) {
	vec3 ld = capsule_b - capsule_a;
	vec3 l0 = capsule_a - pos;
	float a = dot(light_dir, ld);
	float t = clamp(dot(l0, a * light_dir - ld) / (dot(ld, ld) - a * a), 0.0, 1.0);
	vec3 pos_to_ray = capsule_a + t * ld;

	return directional_occlusion_sphere(pos, pos_to_ray, capsule_radius, light_dir, cone_angle);
}

// Check if a point is inside a capsule
bool point_inside_capsule(vec3 point, vec3 capsule_a, vec3 capsule_b, float capsule_radius) {
	vec3 ab = capsule_b - capsule_a;
	vec3 ap = point - capsule_a;

	// Project point onto capsule axis
	float t = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
	vec3 closest_point = capsule_a + t * ab;

	// Check distance from closest point on axis
	float dist = length(point - closest_point);
	return dist <= capsule_radius;
}

// Calculate shadows using capsule proxy meshes
float capsule_shadow(vec3 world_pos, vec3 light_dir, float cone_angle) {
	float shadow = 1.0;

	for (int i = 0; i < capsule_count && i < MAX_CAPSULES; i++) {
		// Check if point is inside the capsule - if so, apply maximum shadow
		if (point_inside_capsule(world_pos, capsule_starts[i], capsule_ends[i], capsule_radii[i])) {
			return 0.0; // Full shadow
		}

		float occlusion = directional_occlusion_capsule(
			world_pos,
			capsule_starts[i],
			capsule_ends[i],
			capsule_radii[i],
			light_dir,
			cone_angle
		);

		// Enhance shadow darkness: 1.0 = no shadow, 0.0 = full shadow
		// Apply power function to make shadows darker and more defined
		float shadow_amount = 1.0 - occlusion;
		shadow_amount = pow(shadow_amount, shadow_falloff) * shadow_strength;
		shadow_amount = clamp(shadow_amount, 0.0, 1.0);

		// Convert back: reduce the light by the shadow amount
		float current_shadow = 1.0 - shadow_amount;
		shadow = min(shadow, current_shadow);
	}

	return shadow;
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

	// Sample and decode light direction with octohedral encoding
	vec3 light_direction_data = texture(light_direction, UV).rgb;
	vec3 light_world_dir = decode_octohedral_direction(light_direction_data);
	float cone_angle = decode_cone_angle(light_direction_data);

	vec3 light_view_dir = mat3(VIEW_MATRIX) * light_world_dir;

	// Calculate shadow using capsule proxies with decoded cone angle
	float shadow = capsule_shadow(
		world_depth_point.xyz,
		light_world_dir,
		cone_angle
	);

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
	
	ALBEDO.rgb = sampled_color_direct * clamp(mix(-100.0, 1.0, shadow), 0.0, 1.0) + sampled_color_indirect * ao;
}
