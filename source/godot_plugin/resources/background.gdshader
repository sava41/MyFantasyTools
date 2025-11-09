shader_type spatial;

render_mode unshaded, fog_disabled;

uniform sampler2D depth_texture : hint_depth_texture;
uniform sampler2D color;
uniform sampler2D depth;

void vertex() {
	POSITION = vec4(VERTEX.xy, 1.0, 1.0);
}

void fragment() {
	float linear_depth = texture(depth, SCREEN_UV).r;
	vec4 clip_pos = PROJECTION_MATRIX * vec4(0.0, 0.0, linear_depth, 1.0);
	clip_pos.xyz /= clip_pos.w;
	DEPTH = -clip_pos.z;

	ALBEDO.rgb = texture(color, SCREEN_UV).bgr;
}
