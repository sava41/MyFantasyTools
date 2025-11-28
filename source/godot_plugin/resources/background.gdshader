shader_type spatial;

render_mode unshaded, fog_disabled;

uniform sampler2D color;
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

const uint sectorCount = 32u;
uniform float sampleCount = 6;
uniform float sampleRadius = 2.0;
uniform float sliceCount = 6;
uniform float hitThickness = 0.3;

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

// ao algo based on GTVBAO
float get_ao(vec2 screen_uv, vec2 aspect, vec3 view_origin, vec3 view_normal, mat4 proj, mat4 inv_proj, float jitter) {

    vec3 view_dir = normalize(-view_origin);

	float sliceRotation = pi / float(sliceCount - 1.0);
    float sampleScale = (-sampleRadius * proj[0][0]) / view_origin.z;
    float sampleOffset = 0.01;
    
	float visibility = 0.0;
    for (float slice = 0.0; slice < sliceCount + 0.5; slice += 1.0) {
        float phi = sliceRotation * (slice + jitter) + pi;
        vec2 omega = vec2(cos(phi), sin(phi));
        vec3 direction = vec3(omega.x, omega.y, 0.0);
        vec3 orthoDirection = direction - dot(direction, view_dir) * view_dir;
        vec3 axis = cross(direction, view_dir);
        vec3 projNormal = view_normal - axis * dot(view_normal, axis);
        float projLength = length(projNormal);

        float signN = sign(dot(orthoDirection, projNormal));
        float cosN = clamp(dot(projNormal, view_dir) / projLength, 0.0, 1.0);
        float n = signN * acos(cosN);

		uint sector_bitmask = 0u;

        for (float currentSample = 0.0; currentSample < sampleCount + 0.5; currentSample += 1.0) {
            float sampleStep = (currentSample + jitter) / sampleCount + sampleOffset;
            vec2 sampleUV = screen_uv - sampleStep * sampleScale * omega * aspect;

			// Skip if outside screen
			if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
				continue;
			}
            
			float depth_sample = texture(depth_buffer, sampleUV).x;

			vec4 buffer_ndc = vec4(sampleUV * 2.0 - 1.0, depth_sample, 1.0);
			vec4 buffer_view = inv_proj * buffer_ndc;
			buffer_view.xyz /= buffer_view.w;
			
            vec3 sampleDistance = buffer_view.xyz - view_origin;
            float sampleLength = length(sampleDistance);

            vec3 sampleHorizon = sampleDistance / sampleLength;

			// Skip samples in the back hemisphere (angle with normal > 90 degrees)
			if (dot(sampleHorizon, view_normal) < 0.0) {
				continue;
			}

			vec2 frontBackHorizon = vec2(0.0);
            frontBackHorizon.x = dot(sampleHorizon, view_dir);
            frontBackHorizon.y = dot(normalize(sampleDistance - view_dir * hitThickness), view_dir);

            frontBackHorizon = acos(frontBackHorizon);
            frontBackHorizon = clamp((frontBackHorizon + n + halfPi) / pi, 0.0, 1.0);

            sector_bitmask = update_sectors(frontBackHorizon.x, frontBackHorizon.y, sector_bitmask);
        }

        visibility += 1.0 - float(bit_count(sector_bitmask)) / float(sectorCount);
    }

    visibility /= sliceCount;

    return visibility;
}

// Decode world-space direction vector from [0,1] RGB to [-1,1] range
// Inverse of encoding: output = input * 2.0 - 1.0
// Also converts from Blender (Z-up) to Godot (Y-up) coordinate system
vec3 decode_direction(vec3 encoded) {
	vec3 dir = encoded * 2.0 - 1.0;
	// Blender: X right, Y forward, Z up
	// Godot:   X right, Y up, Z back
	return vec3(dir.x, dir.z, -dir.y);
}

// Raymarch along light direction and check for depth buffer intersection
float raymarch_shadow(
	vec3 view_origin,
	vec3 view_light_dir,
	mat4 proj_mat,
	mat4 inv_proj_mat
) {
	float step_size = ray_max_distance / float(ray_steps);

	for (int i = 1; i <= ray_steps; i++) {
		// Step along the light direction
		vec3 ray_view = view_origin + view_light_dir * step_size * float(i);

		// Project to screen space
		vec4 clip_pos = proj_mat * vec4(ray_view, 1.0);

		// Skip if behind camera
		if (clip_pos.w <= 0.0) {
			continue;
		}

		clip_pos.xyz /= clip_pos.w;
		vec2 ray_screen_uv = clip_pos.xy * 0.5 + 0.5;

		// Skip if outside screen
		if (ray_screen_uv.x < 0.0 || ray_screen_uv.x > 1.0 || ray_screen_uv.y < 0.0 || ray_screen_uv.y > 1.0) {
			continue;
		}

		// Sample depth buffer
		float buffer_depth = texture(depth_buffer, ray_screen_uv).x;

		// Reconstruct world position from depth buffer
		vec4 buffer_ndc = vec4(clip_pos.xy, buffer_depth, 1.0);
		vec4 buffer_view = inv_proj_mat * buffer_ndc;
		buffer_view.xyz /= buffer_view.w;

		// Compare depths in view space
		float ray_depth = ray_view.z;
		float scene_depth = buffer_view.z;

		// Check if ray is occluded by depth buffer
		// Ray is behind the scene surface (within thickness tolerance)
		if (ray_depth < scene_depth && ray_depth > scene_depth - ray_thickness) {
			return 1.0;
		}
	}

	return 0.0; // Not shadowed
}

void vertex() {
	POSITION = vec4(VERTEX.xy, 1.0, 1.0);
}

void fragment() {

	// Reproject uvs to uncropped uv space

	vec4 ndc = vec4(SCREEN_UV * 2.0 - 1.0, 1.0, 1.0);

	vec4 world_view_point = INV_VIEW_MATRIX * INV_PROJECTION_MATRIX * ndc;
  	world_view_point.xyz /= world_view_point.w;
	world_view_point.w = 1.0;

	vec3 view_dir_uncropped = vec4(inverse(uncropped_view_mat) * world_view_point).xyz;

	float tan_half_fov_h_uncropped = tan(uncropped_fov * 0.5);
	float tan_half_fov_v_uncropped = tan_half_fov_h_uncropped  * uncropped_aspect;

	vec2 ndc_uncropped = view_dir_uncropped.xy / view_dir_uncropped.z;
	ndc_uncropped.x /= tan_half_fov_h_uncropped ;
	ndc_uncropped.y /= tan_half_fov_v_uncropped;
	
	// The x axis is flipped, not sure why
	ndc_uncropped.x = -ndc_uncropped.x;

	vec2 screen_uv_uncropped = ndc_uncropped * 0.5 + 0.5;

	// Reproject depth to cropped projection space
	
	float linear_depth_uncropped = texture(depth, screen_uv_uncropped).x;

	vec3 uncropped_view_depth_pos = -view_dir_uncropped * (linear_depth_uncropped / view_dir_uncropped.z);

	vec4 world_depth_point = uncropped_view_mat * vec4(uncropped_view_depth_pos, 1.0);
	world_depth_point.xyz /= world_depth_point.w;
	world_depth_point.w = 1.0;

	vec4 view_depth_pos = VIEW_MATRIX * world_depth_point;
	view_depth_pos.xyz /= view_depth_pos.w;
	view_depth_pos.w = 1.0;

	vec4 clip_pos = PROJECTION_MATRIX * view_depth_pos;
	clip_pos.xyz /= clip_pos.w;
	DEPTH = clip_pos.z;

	// Sample and decode light direction
	vec3 light_world_dir = decode_direction(texture(light_direction, screen_uv_uncropped).rgb);

	vec3 light_view_dir = mat3(VIEW_MATRIX) * light_world_dir;

	// Raymarch shadow
	float shadow = raymarch_shadow(
		view_depth_pos.xyz,
		light_view_dir,
		PROJECTION_MATRIX,
		INV_PROJECTION_MATRIX
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
	vec3 point_normal = normalize(cross(view_point_ddx, -view_point_ddy));

	vec2 aspect = VIEWPORT_SIZE.yx / VIEWPORT_SIZE.x;
	float jitter = randf(int(FRAGCOORD.x), int(FRAGCOORD.y)) - 0.5;
	float ao = get_ao(SCREEN_UV, aspect, view_depth_pos.xyz, point_normal, PROJECTION_MATRIX, INV_PROJECTION_MATRIX, jitter);

	// Sample texture and clamp to black outside of 0-1 uv range
	vec3 sampled_color = sample_zero_outside(color, screen_uv_uncropped).rgb;
	
	ALBEDO.rgb = sampled_color * vec3(1.0 - shadow * 0.7) * ao * ao * ao * ao;
}
