shader_type spatial;

render_mode unshaded, fog_disabled;

uniform sampler2D color;
uniform sampler2D depth;

uniform float fov = 1.0;
uniform float uncropped_fov = 1.0;
uniform float uncropped_aspect = 1.0;
uniform mat4 uncropped_view_mat = mat4(1.0);

vec4 sample_zero_outside(sampler2D tex, vec2 uv) {
	return (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) 
		? texture(tex, uv) 
		: vec4(0.0);
}

void vertex() {
	POSITION = vec4(VERTEX.xy, 1.0, 1.0);
}

void fragment() {

	// Reproject uvs to uncropped uv space

	vec3 ndc = vec3(SCREEN_UV * 2.0 - 1.0, 1.0);

	vec4 world_view_point = INV_VIEW_MATRIX * INV_PROJECTION_MATRIX * vec4(ndc, 1.0);
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

	vec2 uncropped_uv = ndc_uncropped * 0.5 + 0.5;

	// Sample texture and clamp to black outside of 0-1 range
	
	ALBEDO.rgb = sample_zero_outside(color, uncropped_uv).rgb;

	// Reproject depth to cropped projection space
	
	float linear_depth_uncropped = texture(depth, uncropped_uv).r;

	vec3 uncropped_view_depth_point = view_dir_uncropped * (linear_depth_uncropped / view_dir_uncropped.z);

	vec4 world_depth_point = uncropped_view_mat * vec4(uncropped_view_depth_point, 1.0);
	world_depth_point.xyz /= world_depth_point.w;
	world_depth_point.w = 1.0;

	vec4 clip_pos = PROJECTION_MATRIX * VIEW_MATRIX * world_depth_point;
	clip_pos.xyz /= clip_pos.w;
	DEPTH = -clip_pos.z;
}
