#include <utils/shader.h>
#include <utils/str.h>

static
void shaders_candle_final()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D depth;\n"
		"	sampler2D albedo;\n"
		"	sampler2D nn;\n"
		"	sampler2D mr;\n"
		"	sampler2D emissive;\n"
		"} gbuffer;\n"
		"BUFFER {\n"
		"	sampler2D occlusion;\n"
		"} ssao;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} light;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} refr;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} volum;\n"
		"uniform float ssr_power;\n"
		"uniform float ssao_power;\n");

	str_cat(&shader_buffer,
		"vec4 upsample()\n"
		"{\n"
		"	ivec2 fc = ivec2(gl_FragCoord.xy);\n"
		"	vec3 volumetric = vec3(0.0);\n"
		"	float ssao_factor = 0.0;\n"
		"	float totalWeight = 0.0;\n"
		"	/* Select the closest downscaled pixels. */\n"
		"	int xOffset = fc.x % 2 == 0 ? -1 : 1;\n"
		"	int yOffset = fc.y % 2 == 0 ? -1 : 1;\n"
		"	ivec2 offsets[] = ivec2[](\n"
		"			ivec2(0, 0),\n"
		"			ivec2(0, yOffset),\n"
		"			ivec2(xOffset, 0),\n"
		"			ivec2(xOffset, yOffset));\n"
		"	float frag_depth = linearize(texelFetch(gbuffer.depth, fc, 0).r);\n"
		"	ivec2 dfc = fc / 2;\n");

	str_cat(&shader_buffer,
		"	for (int i = 0; i < 4; i ++)\n"
		"	{\n"
		"		ivec2 coord = clamp(dfc + offsets[i], ivec2(0), ivec2(screen_size) / 2 - 1);\n"
		"		vec3 vol = texelFetch(volum.color, coord, 0).rgb;\n"
		"		float dep = linearize(texelFetch(gbuffer.depth, coord * 2, 0).r);\n"
		"		float w = max(0.0, 1.0 - 0.5 * abs(dep - frag_depth));\n"
		"		if (ssao_power > 0.05)\n"
		"		{\n"
		"			vec2 sampled = texelFetch(ssao.occlusion, coord, 0).rg;\n"
		"			float downscaledSsao = sampled.r;\n"
		"			ssao_factor += (1.0 - downscaledSsao) * w;\n"
		"		}\n");
	str_cat(&shader_buffer,
		"		volumetric += vol * w;\n"
		"		totalWeight += w;\n"
		"	}\n"
		"	const float epsilon = 0.0001;\n"
		"	float factor = totalWeight + epsilon;\n"
		"	return vec4(volumetric, (factor - ssao_factor) * ssao_power) / factor;\n"
		"}\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	ivec2 fc = ivec2(gl_FragCoord.xy);\n"
		"	vec4 cc = texelFetch(light.color, fc, 0);\n"
		"	vec2 normal = texelFetch(gbuffer.nn, fc, 0).rg;\n"
		"	vec2 metalic_roughness = texelFetch(gbuffer.mr, fc, 0).rg;\n"
		"	vec4 albedo = texelFetch(gbuffer.albedo, fc, 0);\n"
		"	vec3 emissive = texelFetch(gbuffer.emissive, fc, 0).rgb;\n"
		"	vec3 nor = decode_normal(normal);\n");

	str_cat(&shader_buffer,
		"	vec4 ssred = ssr_power > 0.05 ? ssr2(gbuffer.depth, refr.color, albedo,\n"
		"			metalic_roughness, nor) * 1.5 : vec4(0.0);\n"
		"	float frag_depth = linearize(texelFetch(gbuffer.depth, fc, 0).r);\n"
		"	vec4 volum_ssao = upsample();\n"
		"	cc.rgb *= volum_ssao.w;\n"
		"	vec3 final = cc.rgb + ssred.rgb * (ssred.a * ssr_power) + emissive + volum_ssao.xyz;\n"
		"	final = final * pow(2.0, camera(exposure));\n"
		"	FragColor = vec4(final, 1.0);\n"
		"}\n");

	shader_add_source("candle:final.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_ssao()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"#define ITERS 4.0\n"
		"#define TAPS 7u\n"
		"BUFFER {\n"
		"	sampler2D depth;\n"
		"	sampler2D nn;\n"
		"	sampler2D mr;\n"
		"} gbuffer;\n"
		"layout (location = 0) out float FragColor;\n"
		"vec2 hemicircle[] = vec2[](\n"
		"	vec2(1.0, 0.0),\n"
		"	vec2(0.92388, 0.38268),\n"
		"	vec2(0.70711, 0.70711),\n"
		"	vec2(0.38268, 0.92388),\n"
		"	vec2(0.0, 1.0),\n"
		"	vec2(-0.38268, 0.92388),\n"
		"	vec2(-0.70711, 0.70711),\n"
		"	vec2(-0.92388, 0.38268)\n"
		");\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	float ao = 0.0;\n"
		"	vec2 tc = texcoord;\n"
		"	vec3 n = decode_normal(texelFetch(gbuffer.nn, ivec2(gl_FragCoord.xy) * 2, 0).rg);\n"
		"	float D = texelFetch(gbuffer.depth, ivec2(gl_FragCoord.xy) * 2, 0).r;\n"
		"	float d0 = linearize(D);\n"
		"	float dither = dither_value();\n"
		"	float rad = (0.4 / d0);\n"
		"	vec2 rnd = normalize(vec2(rand(tc), dither));\n"
		"	float z = clamp((n.z + 0.5), 0.0, 1.0);\n");

	str_cat(&shader_buffer,
		"	for (uint j = 0u; j < TAPS; ++j)\n"
		"	{\n"
		"		float angle1 = -M_PI;\n"
		"		float angle2 = -M_PI;\n"
		"		float angle = M_PI * 2.0;\n"
		"		vec2 offset = reflect(hemicircle[j], rnd) * rad;\n"
		"		vec2 norm = normalize(n.xy);\n"
		"		float d = (norm.x * offset.x + norm.y * offset.y);\n"
		"		norm = norm * d;\n"
		"		offset = offset - norm + norm * (1.0 - (1.0 - z));\n");

	str_cat(&shader_buffer,
		"		for (float i = 0.0; i < ITERS; ++i)\n"
		"		{\n"
		"			float c0 = pow((i + dither) / (ITERS - 1.0), 2.0) + 0.001;\n"
		"			vec2 coord1 = offset * c0;\n"
		"			ivec2 tp = ivec2((tc + coord1) * (screen_size)) * 2;\n"
		"			ivec2 tn = ivec2((tc - coord1) * (screen_size)) * 2;\n"
		"			float d1 = linearize(texelFetch(gbuffer.depth, tp, 0).r);\n"
		"			float d2 = linearize(texelFetch(gbuffer.depth, tn, 0).r);\n"
		"			float c1 = d0 - d1;\n"
		"			float c2 = d0 - d2;\n");

	str_cat(&shader_buffer,
		"			if (abs(c1) < 1.0)\n"
		"				angle1 = atan(c1, c0);\n"
		"			else if (abs(c1) < 2.0)\n"
		"				angle1 += (atan(c1, c0) - angle1) * (1.0 - (abs(c1) - 1.0));\n"
		"			if (abs(c2) < 1.0)\n"
		"				angle2 = atan(c2, c0);\n"
		"			else if (abs(c2) < 2.0)\n"
		"				angle2 += (atan(c2, c0) - angle2) * (1.0 - (abs(c2) - 1.0));\n"
		"			angle = min(angle, M_PI - angle1 - angle2);\n");

	str_cat(&shader_buffer,
		"			float falloff = (1.0 + max(abs(c1), abs(c2)));\n"
		"			ao += clamp(sin(angle) / falloff, 0.0, 1.0);\n"
		"		}\n"
		"	}\n"
		"	ao = 1.0 - (ao / (float(TAPS) * ITERS));\n"
		"	FragColor = clamp(ao, 0.0, 1.0); \n"
		"}\n");

	shader_add_source("candle:ssao.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_copy()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"uniform int level;\n"
		"void main(void)\n"
		"{\n"
		"	vec4 tex = texelFetch(buf.color, ivec2(gl_FragCoord.xy), level);\n"
		"	if(tex.a == 0.0) discard;\n"
		"	FragColor = tex;\n"
		"}\n");

	shader_add_source("candle:copy.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_downsample()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"uniform int level;\n"
		"void main(void)\n"
		"{\n"
		"	vec2 pp = vec2(gl_FragCoord.xy) * 2.0;\n"
		"	vec4 tex = vec4(0.0);\n"
		"	ivec2 iv;\n"
		"	for(iv.x = 0; iv.x < 2; iv.x++)\n"
		"		for(iv.y = 0; iv.y < 2; iv.y++)\n"
		"			tex += texelFetch(buf.color, ivec2(pp) + iv, level);\n"
		"	tex /= 4.0;\n"
		"	FragColor = tex;\n"
		"}\n");

	shader_add_source("candle:downsample.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_upsample()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"uniform int level;\n"
		"uniform float alpha;\n"
		"void main(void)\n"
		"{\n"
		"	vec4 tex;\n"
		"	tex = textureLod(buf.color, pixel_pos(), float(level));\n"
		"	tex.a = alpha;\n"
		"	FragColor = tex;\n"
		"}\n");

	shader_add_source("candle:upsample.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}


static
void shaders_candle_kawase()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"layout (location = 0) out vec4 FragColor;\n"
		"uniform int distance;\n"
		"uniform int level;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"vec3 fs(ivec2 coord)\n"
		"{\n"
		"	if(coord.x < 0) coord.x = 0;\n"
		"	if(coord.y < 0) coord.y = 0;\n"
		"	ivec2 s = textureSize(buf.color, level);\n"
		"	if(coord.x >= s.x) coord.x = s.x - 1;\n"
		"	if(coord.y >= s.y) coord.y = s.y - 1;\n"
		"	return texelFetch(buf.color, coord, level).rgb;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 fetch(ivec2 coord)\n"
		"{\n"
		"	return fs(coord).rgb +\n"
		"		   fs(coord + ivec2(1, 0)) +\n"
		"		   fs(coord + ivec2(1, 1)) +\n"
		"		   fs(coord + ivec2(0, 1));\n"
		"}\n"
		"void main(void)\n"
		"{\n"
		"	ivec2 tc = ivec2(gl_FragCoord.xy);\n"
		"	vec3 c = vec3(0.0);\n"
		"	c += fetch(tc + ivec2(distance, distance));\n"
		"	c += fetch(tc + ivec2(-distance - 1, -distance - 1));\n"
		"	c += fetch(tc + ivec2(distance, -distance - 1));\n"
		"	c += fetch(tc + ivec2(-distance - 1, distance));\n"
		"	FragColor = vec4(c / 16.0, 1.0);\n"
		"}\n");

	shader_add_source("candle:kawase.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_bright()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"void main(void)\n"
		"{\n"
		"	float brightPassThreshold = 0.99;\n"
		"	vec3 luminanceVector = vec3(0.2125, 0.7154, 0.0721);\n"
		"	vec3 c = textureLod(buf.color, texcoord, 0.0).rgb;\n"
		"    float luminance = dot(luminanceVector, c);\n"
		"    luminance = max(0.0, luminance - brightPassThreshold);\n"
		"    c *= sign(luminance);\n"
		"    FragColor = vec4(c, 1.0);\n"
		"}\n");

	shader_add_source("candle:bright.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_framebuffer_draw()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 Alb;\n"
		"layout (location = 1) out vec4 NN;\n"
		"layout (location = 2) out vec4 MR;\n"
		"layout (location = 3) out vec3 Emi;\n"
		"void main()\n"
		"{\n"
		"	vec4 dif = textureLod(g_framebuffer, texcoord, 0.0);\n"
		"	Alb = dif;\n"
		"	if(dif.a < 0.7) discard;\n"
		"	NN = vec4(encode_normal(get_normal(vec3(0.5, 0.5, 1.0))), 0.0, 1.0);\n"
		"	MR = vec4(0.5, 0.5, 0.0, 1.0);\n"
		"	Emi = vec3(0.0, 0.0, 0.0);\n"
		"}\n");

	shader_add_source("candle:framebuffer_draw.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_quad()
{
	/* TODO: Should probably be moved to systems/window.c */
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"uniform sampler2D tex;\n"
		"void main(void)\n"
		"{\n"
		"	FragColor = textureLod(tex, texcoord, 0.0);\n"
		"}  \n");

	shader_add_source("candle:quad.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_motion()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n"
		"BUFFER {\n"
		"	sampler2D depth;\n"
		"	sampler2D albedo;\n"
		"} gbuffer;\n"
		"uniform float power;\n");
	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	vec2 p = pixel_pos();\n"
		"	vec3 c_pos = get_position(gbuffer.depth, p);\n"
		"	vec4 w_pos = (camera(model) * vec4(c_pos, 1.0));\n"
		"	vec4 previousPos = (camera(projection) * camera(previous_view)) * w_pos;\n"
		"	vec2 pp = previousPos.xy / previousPos.w;\n"
		"	const int num_samples = 4;\n"
		"	pp = (pp + 1.0) / 2.0;\n"
		"	vec2 velocity = ((p - pp) / float(num_samples)) * power;\n"
		"	float samples = 0.0;\n");
	str_cat(&shader_buffer,
		"	p += velocity * dither_value();\n"
		"	vec3 color = vec3(0.0);\n"
		"	for(int i = 0; i < num_samples; ++i)\n"
		"	{\n"
		"		samples += 1.0;\n"
		"		color += textureLod(buf.color, p, 0.0).rgb;\n"
		"		p += velocity;\n"
		"		if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0)\n"
		"		{\n"
		"			break;\n"
		"		}\n"
		"	}\n"
		"	FragColor = vec4(color / samples, 1.0);\n"
		"}\n");

	shader_add_source("candle:motion.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_uniforms()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#ifndef UNIFORMS\n"
		"#define UNIFORMS\n"
		"struct camera_t\n"
		"{\n"
		"	mat4 previous_view;\n"
		"	mat4 model;\n"
		"	mat4 view;\n"
		"	mat4 projection;\n"
		"	mat4 inv_projection;\n"
		"	vec3 pos;\n"
		"	float exposure;\n"
		"};\n");
	str_cat(&shader_buffer,
		"struct light_t\n"
		"{\n"
		"	vec3 color;\n"
		"	float volumetric;\n"
		"	uvec2 pos;\n"
		"	int lod;\n"
		"	float radius;\n"
		"};\n"
		"layout(std140) uniform renderer_t\n"
		"{\n"
		"	camera_t camera;\n"
		"} renderer;\n");
	str_cat(&shader_buffer,
		"layout(std140) uniform scene_t\n"
		"{\n"
		"	light_t lights[62];\n"
		"	vec3 test_color;\n"
		"	float time;\n"
		"} scene;\n"
		"#ifdef HAS_SKIN\n"
		"layout(std140) uniform skin_t\n"
		"{\n"
		"	mat4 bones[30];\n"
		"} skin;\n"
		"#endif\n");
	str_cat(&shader_buffer,
		/* layout(location = 22) */ "uniform vec2 screen_size;\n"
		/* layout(location = 23) */ "uniform bool has_tex;\n"
		/* layout(location = 24) */ "uniform bool has_normals;\n"
		/* layout(location = 25) */ "uniform bool receive_shadows;\n"
		/* layout(location = 26) */ "uniform sampler2D g_cache;\n"
		/* layout(location = 27) */ "uniform sampler2D g_indir;\n"
		/* layout(location = 28) */ "uniform sampler2D g_probes;\n"
		/* layout(location = 29) */ "uniform sampler2D g_framebuffer;\n"
		"#define light(prop) (scene.lights[matid].prop)\n"
		"#define camera(prop) (renderer.camera.prop)\n"
		"#endif\n");
	shader_add_source("candle:uniforms.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_common()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#ifndef FRAG_COMMON\n"
		"#define FRAG_COMMON\n"
		"#include \"candle:uniforms.glsl\"\n"
		"\n"
		"const float M_PI = 3.141592653589793;\n"
		"const float c_MinRoughness = 0.04;\n"
		"\n"
		"flat in uvec2 id;\n"
		"flat in uint matid;\n"
		"flat in vec2 object_id;\n"
		"flat in uvec2 poly_id;\n"
		"flat in vec3 obj_pos;\n"
		"flat in mat4 model;\n"
		"\n"
		"in vec3 poly_color;\n"
		"in vec3 vertex_position;\n"
		"in vec3 vertex_tangent;\n"
		"in vec3 vertex_normal;\n"
		"in vec3 vertex_world_position;\n"
		"in vec2 texcoord;\n"
		"\n");

	str_cat(&shader_buffer,
		"vec2 sampleCube(const vec3 v, out uint faceIndex, out float z)\n"
		"{\n"
		"	vec3 vAbs = abs(v);\n"
		"	float ma;\n"
		"	vec2 uv;\n"
		"	if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)\n"
		"	{\n"
		"		faceIndex = v.z < 0.0 ? 5u : 4u;\n"
		"		ma = 0.5 / vAbs.z;\n"
		"		uv = vec2(v.z < 0.0 ? -v.x : v.x, -v.y);\n"
		"		z = vAbs.z;\n"
		"	}\n"
		"	else if(vAbs.y >= vAbs.x)\n");
	str_cat(&shader_buffer,
		"	{\n"
		"		faceIndex = v.y < 0.0 ? 3u : 2u;\n"
		"		ma = 0.5 / vAbs.y;\n"
		"		uv = vec2(v.x, v.y < 0.0 ? -v.z : v.z);\n"
		"		z = vAbs.y;\n"
		"	}\n"
		"	else\n"
		"	{\n"
		"		faceIndex = v.x < 0.0 ? 1u : 0u;\n"
		"		ma = 0.5 / vAbs.x;\n"
		"		uv = vec2(v.x < 0.0 ? v.z : -v.z, -v.y);\n"
		"		z = vAbs.x;\n"
		"	}\n"
		"	return uv * ma + 0.5;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec2 pixel_pos()\n"
		"{\n"
		"	return gl_FragCoord.xy / screen_size;\n"
		"}\n");

	str_cat(&shader_buffer,
		"/* Does not take into account GL_TEXTURE_MIN_LOD/GL_TEXTURE_MAX_LOD/GL_TEXTURE_LOD_BIAS, */\n"
		"/* nor implementation-specific flexibility allowed by OpenGL spec */\n"
		"float mip_map_scalar(in vec2 texture_coordinate) /* in texel units */\n"
		"{\n"
		"    vec2 dx_vtc = dFdx(texture_coordinate);\n"
		"    vec2 dy_vtc = dFdy(texture_coordinate);\n"
		"    return max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));\n"
		"}\n");

	str_cat(&shader_buffer,
		"#define MAX_MIPS 9u\n"
		"float mip_map_level(in vec2 texture_coordinate) // in texel units\n"
		"{\n"
		"	return clamp(0.5 * log2(mip_map_scalar(texture_coordinate)), 0.0, float(MAX_MIPS - 1u));\n"
		"}\n"
		"\n"
		"#define g_indir_w 128u\n"
		"#define g_indir_h 128u\n"
		"#define g_cache_w 64u\n"
		"#define g_cache_h 32u\n"
		"\n"
		"uint round_power_of_two(uint v)\n"
		"{\n"
		"	v--;\n"
		"	v |= v >> 1u;\n"
		"	v |= v >> 2u;\n"
		"	v |= v >> 4u;\n"
		"	v |= v >> 8u;\n"
		"	v |= v >> 16u;\n"
		"	return v + 1u;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec4 solve_mip(uint tiles_per_row, uint base_tile, uint mip, vec2 coords,\n"
		"               out uint tile_out)\n"
		"{\n"
		"	float max_mips = log2(float(tiles_per_row));\n"
		"	float mm = min(max_mips, float(mip));\n"
		"	uint map_tiles = uint(exp2(2. * (max_mips + 1. - mm)) * (exp2(mm * 2.) - 1.)) / 3u;\n"
		"	map_tiles += uint(max(0., float(mip) - max_mips));\n"
		"	uint offset = base_tile + map_tiles;\n"
		"	uint mip_tpr = tiles_per_row >> mip;\n"
		"	uvec2 indir_coords = uvec2(floor(coords / (exp2(float(mip)) * 128.0)));\n");
	str_cat(&shader_buffer,
		"	uint tile_id = indir_coords.y * mip_tpr + indir_coords.x + offset;\n"
		"	tile_out = tile_id;\n"
		"	vec3 info = texelFetch(g_indir, ivec2(tile_id % g_indir_w,\n"
		"	                       tile_id / g_indir_w), 0).rgb * 255.0;\n"
		"	float actual_mip = info.b;\n"
		"	const vec2 g_cache_size = vec2(g_cache_w, g_cache_h);\n"
		"	vec2 tex_coords = coords / exp2(actual_mip);\n"
		"	vec2 tile_coords = mod(tex_coords, 128.0) + 0.5f;\n"
		"	return textureLod(g_cache,\n"
		"			(info.xy + tile_coords / 129.) / g_cache_size, 0.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec4 textureSVT(uvec2 size, uint base_tile, vec2 coords, out uint tile_out,\n"
		"                float mip_scale)\n"
		"{\n"
		"	coords.y = 1.0 - coords.y;\n"
		"	vec2 rcoords = fract(coords) * vec2(size);\n"
		"	uint max_dim = uint(ceil(float(max(size.x, size.y)) / 128.0));\n"
		"	uint tiles_per_row = round_power_of_two(max_dim);\n");
	str_cat(&shader_buffer,
		"	float mip = mip_map_level(coords * vec2(size) * mip_scale);\n"
		"	uint mip0 = uint(floor(mip));\n"
		"	uint mip1 = uint(ceil(mip));\n"
		"	uint ignore;\n"
		"	return mix(solve_mip(tiles_per_row, base_tile, mip0, rcoords, tile_out),\n"
		"	           solve_mip(tiles_per_row, base_tile, mip1, rcoords, ignore), fract(mip));\n"
		"}\n");

	str_cat(&shader_buffer,
		"#define NEAR 0.1\n"
		"#define FAR 1000.0\n"
		"float linearize(float depth)\n"
		"{\n"
		"    return (2.0 * NEAR * FAR) / ((FAR + NEAR) - (2.0 * depth - 1.0) * (FAR - NEAR));\n"
		"}\n"
		"float unlinearize(float depth)\n"
		"{\n"
		"	return FAR * (1.0 - (NEAR / depth)) / (FAR - NEAR);\n"
		"}\n");

	str_cat(&shader_buffer,
		"const vec4 bitSh = vec4(256. * 256. * 256., 256. * 256., 256., 1.);\n"
		"const vec4 bitMsk = vec4(0.,vec3(1./256.0));\n"
		"const vec4 bitShifts = vec4(1.) / bitSh;\n"
		"\n"
		"vec4 encode_float_rgba_unit( float v ) {\n"
		"	vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;\n"
		"	enc = fract(enc);\n"
		"	enc -= enc.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);\n"
		"	return enc;\n"
		"}\n");
	str_cat(&shader_buffer,
		"float decode_float_rgba_unit( vec4 rgba ) {\n"
		"	return dot(rgba, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0) );\n"
		"}\n"
		"vec4 encode_float_rgba (float value) {\n"
		"	value /= 256.0;\n"
		"    vec4 comp = fract(value * bitSh);\n"
		"    comp -= comp.xxyz * bitMsk;\n"
		"    return comp;\n"
		"}\n"
		"\n"
		"float decode_float_rgba (vec4 color) {\n"
		"    return dot(color, bitShifts) * (256.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"float dither_value()\n"
		"{\n"
		"	const float pattern[16] = float[16](0.0f, 0.5f, 0.125f, 0.625f,\n"
		"		0.75f, 0.22f, 0.875f, 0.375f, 0.1875f, 0.6875f, 0.0625f, 0.5625,\n"
		"		0.9375f, 0.4375f, 0.8125f, 0.3125);\n"
		"	return pattern[  (int(gl_FragCoord.x) % 4) * 4\n"
		"	               + (int(gl_FragCoord.y) % 4)];\n"
		"}\n");

	/* TODO: move to phong.glsl */
	str_cat(&shader_buffer,
		"float lookup_single(vec3 shadowCoord, out float z)\n"
		"{\n"
		"	uint size = 1024u / uint(pow(2.0, float(light(lod))));\n"
		"	uint cube_layer;\n"
		"	uvec2 tc = uvec2(floor(sampleCube(shadowCoord, cube_layer, z) * float(size)));\n"
		"	uvec2 pos = uvec2(cube_layer % 2u, cube_layer / 2u) * size;\n"
		"	vec4 distance = texelFetch(g_probes, ivec2(tc + pos + light(pos)), 0);\n"
		"	return linearize(decode_float_rgba(distance));\n"
		"}\n");

	str_cat(&shader_buffer,
		"float lookup(vec3 coord)\n"
		"{\n"
		"	/* float dist = length(coord); */\n"
		"	float z;\n"
		"	float dist = lookup_single(coord, z);\n"
		"	return (dist > z - 0.08) ? 1.0 : 0.0;\n"
		"}\n");

	str_cat(&shader_buffer,
		"float get_shadow(vec3 vec, float point_to_light, float dist_to_eye, float depth)\n"
		"{\n"
		"	vec3 taps[] = vec3[](\n"
		"		vec3(0.068824, -0.326151,   0.3),\n"
		"		vec3(0.248043, 0.222679,   0.3),\n"
		"		vec3(-0.316867, 0.103472,   0.3),\n"
		"		vec3(-0.525182, 0.410644,   0.6),\n"
		"		vec3(-0.618219, -0.249499,   0.6),\n"
		"		vec3(-0.093037, -0.660143,   0.6),\n"
		"		vec3(0.525182, -0.410644,   0.6),\n"
		"		vec3(0.618219, 0.249499,   0.6),\n");
	str_cat(&shader_buffer,
		"		vec3(0.093037, 0.660143,   0.6),\n"
		"		vec3(0.536822, -0.843695,   1.0),\n"
		"		vec3(0.930210, -0.367028,   1.0),\n"
		"		vec3(0.968289, 0.249832,   1.0),\n"
		"		vec3(0.636515, 0.771264,   1.0),\n"
		"		vec3(0.061613, 0.998100,   1.0),\n"
		"		vec3(-0.536822, 0.843695,   1.0),\n"
		"		vec3(-0.930210, 0.367028,   1.0),\n"
		"		vec3(-0.968289, -0.249832,   1.0),\n"
		"		vec3(-0.636515, -0.771264,   1.0),\n"
		"		vec3(-0.061613, -0.998100,   1.0)\n"
		"	);\n");
	str_cat(&shader_buffer,
		"	float z;\n"
		"	float ocluder_to_light = lookup_single(-vec, z);\n"
		"	if (ocluder_to_light > z - 0.08) return 0.0;\n"
		"	ocluder_to_light = min(z, ocluder_to_light);\n"
		"	float dither = dither_value();\n"
		"	float shadow_len = min(0.8 * (point_to_light / ocluder_to_light - 1.0), 10.0);\n"
		"	if(shadow_len > 0.001)\n"
		"	{\n"
		"		vec3 normal = normalize(vec);\n"
		"		vec3 tangent = cross(normal, vec3(0.0, 1.0, 0.0));\n"
		"		vec3 bitangent = cross(normal, tangent);\n");
	str_cat(&shader_buffer,
		"		float min_dist = 1.0;\n"
		"		for (uint j = 0u; j < 19u; j++)\n"
		"		{\n"
		"			vec3 offset = taps[j] * ((4.0/3.0) * dither);\n"
		"			if(lookup(-vec + (offset.x * tangent + offset.y * bitangent) * shadow_len) > 0.5)\n"
		"			{\n"
		"				min_dist = offset.z;\n"
		"				break;\n"
		"			}\n"
		"		}\n"
		"		return min_dist;\n"
		"	}\n"
		"	return 0.0;\n"
		"}\n");

	str_cat(&shader_buffer,
		/* SPHEREMAP TRANSFORM */
		/* https://aras-p.info/texts/CompactNormalStorage.html */
		/* vec2 encode_normal(vec3 n) */
		/* { */
		/*     vec2 enc = normalize(n.xy) * (sqrt(-n.z*0.5+0.5)); */
		/*     enc = enc*0.5+0.5; */
		/*     return enc; */
		/* } */
		/* vec3 decode_normal(vec2 enc) */
		/* { */
		/*     vec4 nn = vec4(enc, 0.0, 0.0) * vec4(2.0, 2.0, 0.0, 0.0) + vec4(-1.0, -1.0, 1.0, -1.0); */
		/*     float l = dot(nn.xyz, -nn.xyw); */
		/*     nn.z = l; */
		/*     nn.xy *= sqrt(l); */
		/*     return nn.xyz * 2.0 + vec3(0.0, 0.0, -1.0); */
		/* } */
		"vec2 encode_normal(vec3 n)\n"
		"{\n"
		"    float p = sqrt(n.z * 8.0 + 8.0);\n"
		"    return vec2(n.xy/p + 0.5);\n"
		"}\n"
		"vec3 decode_normal(vec2 enc)\n"
		"{\n"
		"    vec2 fenc = enc * 4.0 - 2.0;\n"
		"    float f = dot(fenc,fenc);\n"
		"    float g = sqrt(1.0 - f / 4.0);\n"
		"    vec3 n;\n"
		"    n.xy = fenc * g;\n"
		"    n.z = 1.0 - f / 2.0;\n"
		"    return n;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 get_position(sampler2D depth)\n"
		"{\n"
		"	vec2 pos = pixel_pos();\n"
		"	vec3 raw_pos = vec3(pos, textureLod(depth, pos, 0.0).r);\n"
		"	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);\n"
		"	vec4 view_pos = camera(inv_projection) * clip_pos;\n"
		"	return view_pos.xyz / view_pos.w;\n"
		"}\n"
		"vec3 get_position(float depth, vec2 pos)\n"
		"{\n"
		"	vec3 raw_pos = vec3(pos, depth);\n"
		"	vec4 clip_pos = vec4(raw_pos * 2.0 - 1.0, 1.0);\n"
		"	vec4 view_pos = camera(inv_projection) * clip_pos;\n"
		"	return view_pos.xyz / view_pos.w;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 get_position(sampler2D depth, vec2 pos)\n"
		"{\n"
		"	return get_position(textureLod(depth, pos, 0.0).r, pos);\n"
		"}\n"
		"vec3 get_normal(sampler2D buffer)\n"
		"{\n"
		"	return decode_normal(texelFetch(buffer, ivec2(gl_FragCoord.x,\n"
		"					gl_FragCoord.y), 0).rg);\n"
		"}\n"
		"mat3 TM()\n"
		"{\n"
		"	vec3 vertex_bitangent = cross(normalize(vertex_tangent), normalize(vertex_normal));\n"
		"	return mat3(vertex_tangent, vertex_bitangent, vertex_normal);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 get_normal(vec3 color)\n"
		"{\n"
		"	vec3 norm;\n"
		"	if(has_tex)\n"
		"	{\n"
		"		vec3 texcolor = color * 2.0 - 1.0;\n"
		"		norm = TM() * texcolor;\n"
		"	}\n"
		"	else\n"
		"	{\n"
		"		norm = vertex_normal;\n"
		"	}\n"
		"	if(!gl_FrontFacing)\n"
		"	{\n"
		"		norm = -norm;\n"
		"	}\n"
		"	return normalize(norm);\n"
		"}\n");

	str_cat(&shader_buffer,
		"float rand(vec2 co)\n"
		"{\n"
		"    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);\n"
		"}\n");

	/* TODO: should go to ssr.glsl */
	str_cat(&shader_buffer,
		"vec3 get_proj_coord(sampler2D depthmap, vec3 hitCoord)\n"
		"	/* z = hitCoord.z - depth */\n"
		"{\n"
		"	vec4 projectedCoord     = camera(projection) * vec4(hitCoord, 1.0);\n"
		"	projectedCoord.xy      /= projectedCoord.w;\n"
		"	projectedCoord.xy       = projectedCoord.xy * 0.5 + 0.5;\n"
		"	float depth             = get_position(depthmap, projectedCoord.xy).z;\n"
		"	return vec3(projectedCoord.xy, hitCoord.z - depth);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 BinarySearch(sampler2D depthmap, vec3 dir, inout vec3 hitCoord)\n"
		"{\n"
		"	float depth;\n"
		"	vec3 pc;\n"
		"	for(uint i = 0u; i < 16u; i++)\n"
		"	{\n"
		"		pc = get_proj_coord(depthmap, hitCoord);\n"
		"		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;\n"
		"		if(abs(pc.z) <= 0.01)\n"
		"		{\n"
		"			return vec3(pc.xy, 1.0);\n"
		"		}\n"
		"		dir *= 0.5;\n"
		"		hitCoord -= dir;\n"
		"	}\n"
		"	return vec3(pc.xy, 1.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 RayCast(sampler2D depth, vec3 dir, inout vec3 hitCoord)\n"
		"{\n"
		"	dir *= 0.1;  \n"
		"	for(uint i = 0u; i < 64u; ++i) {\n"
		"		hitCoord += dir; \n"
		"		dir *= 1.1;\n"
		"		vec3 pc = get_proj_coord(depth, hitCoord);\n"
		"		if(pc.x > 1.0 || pc.y > 1.0 || pc.x < 0.0 || pc.y < 0.0) break;\n"
		"		if(pc.z < -2.0) break;\n"
		"		if(pc.z < 0.0)\n"
		"		{\n"
		"			return vec3(pc.xy, 1.0);\n"
		"			/* return BinarySearch(dir, hitCoord); */\n"
		"		}\n"
		"	}\n"
		"	return vec3(0.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"float roughnessToSpecularPower(float r)\n"
		"{\n"
		"  return pow(1.0 - r, 2.0);\n"
		"}\n"
		"float specularPowerToConeAngle(float specularPower)\n"
		"{\n"
		"	const float xi = 0.244;\n"
		"	float exponent = 1.0 / (specularPower + 1.0);\n"
		"	return acos(pow(xi, exponent));\n"
		"}\n");

	str_cat(&shader_buffer,
		"#define CNST_1DIVPI 0.31830988618\n"
		"float isoscelesTriangleInRadius(float a, float h)\n"
		"{\n"
		"	float a2 = a * a;\n"
		"	float fh2 = 4.0 * h * h;\n"
		"	return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);\n"
		"}\n"
		"vec3 fresnelSchlick(vec3 F0, float cosTheta)\n"
		"{\n"
		"	return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec4 ssr2(sampler2D depth, sampler2D screen, vec4 base_color,\n"
		"		vec2 metallic_roughness, vec3 nor)\n"
		"{\n"
		"	vec2 tc = pixel_pos();\n"
		"	vec3 pos = get_position(depth, pixel_pos());\n"
		"	float perceptualRoughness = metallic_roughness.y;\n"
		"	float metallic = metallic_roughness.x;\n"
		"	perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);\n"
		"	if(perceptualRoughness > 0.95) return vec4(0.0);\n"
		"	float gloss = 1.0 - perceptualRoughness;\n"
		"	float specularPower = roughnessToSpecularPower(perceptualRoughness);\n");
	str_cat(&shader_buffer,
		"	perceptualRoughness *= 0.3;\n"
		"	vec3 w_pos = (camera(model) * vec4(pos, 1.0)).xyz;\n"
		"	vec3 w_nor = (camera(model) * vec4(nor, 0.0)).xyz;\n"
		"	vec3 c_pos = camera(pos) - w_pos;\n"
		"	vec3 eye_dir = normalize(-c_pos);\n"
		"	vec3 reflected = normalize(reflect(pos, nor));\n");
	str_cat(&shader_buffer,
		"	/* Ray cast */\n"
		"	vec3 hitPos = pos.xyz; // + vec3(0.0, 0.0, rand(camPos.xz) * 0.2 - 0.1);\n"
		"	vec3 coords = RayCast(depth, reflected, hitPos);\n"
		"	vec2 dCoords = abs(vec2(0.5, 0.5) - coords.xy) * 2.0;\n"
		"	float screenEdgefactor = (clamp(1.0 -\n"
		"				(pow(dCoords.x, 4.0) + pow(dCoords.y, 4.0)), 0.0, 1.0));\n"
		"	vec3 fallback_color = vec3(0.0);\n"
		"	vec2 deltaP = coords.xy - texcoord;\n");
	str_cat(&shader_buffer,
		"	float adjacentLength = length(deltaP);\n"
		"	vec2 adjacentUnit = normalize(deltaP);\n"
		"	uint numMips = MAX_MIPS;\n"
		"	vec4 reflect_color = vec4(0.0);\n"
		"	float maxMipLevel = 3.0;\n"
		"	float glossMult = gloss;\n");
	str_cat(&shader_buffer,
		"	for(int i = -4; i <= 4; ++i)\n"
		"	{\n"
		"		float len = adjacentLength * (float(i) / 4.0) * perceptualRoughness * 0.3;\n"
		"		vec2 samplePos = coords.xy + vec2(adjacentUnit.y, -adjacentUnit.x) * len;\n"
		"		float mipChannel = perceptualRoughness * adjacentLength * 40.0;\n"
		"		glossMult = 1.0;\n"
		"		vec4 newColor = textureLod(screen, samplePos, mipChannel).rgba;\n"
		"		reflect_color += clamp(newColor / 15.0, 0.0, 1.0);\n"
		"		glossMult *= gloss;\n"
		"	}\n");
	str_cat(&shader_buffer,
		"	vec3 f0 = vec3(0.04);\n"
		"	vec3 specular_color = mix(f0, base_color.rgb, metallic);\n"
		"	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);\n"
		"	float NdotL = clamp(dot(w_nor, -eye_dir), 0.001, 1.0);\n"
		"	specular_color = fresnelSchlick(vec3(reflectance), NdotL) * CNST_1DIVPI;\n"
		"	float fade_on_roughness = 1.0;\n"
		"	float fade = screenEdgefactor * fade_on_roughness;\n"
		"	return vec4(mix(fallback_color,\n"
		"				reflect_color.xyz * specular_color, fade), 1.0);\n"
		"}\n");

	/* TODO: should go to pbr.glsl*/
	/* FROM https://github.com/KhronosGroup/glTF-WebGL-PBR */
	str_cat(&shader_buffer,
		"struct PBRInfo\n"
		"{\n"
		"    float NdotL;\n"              /* cos angle between normal and light direction */
		"    float NdotV;\n"              /* cos angle between normal and view direction */
		"    float NdotH;\n"              /* cos angle between normal and half vector */
		"    float LdotH;\n"              /* cos angle between light direction and half vector */
		"    float VdotH;\n"              /* cos angle between view direction and half vector */
		"    float perceptualRoughness;\n"/* roughness value, as authored by the model creator (input to shader) */
		"    float metalness;\n"          /* metallic value at the surface */
		"    vec3 reflectance0;\n"        /* full reflectance color (normal incidence angle) */
		"    vec3 reflectance90;\n"       /* reflectance color at grazing angle */
		"    float alpha_roughness_sq;\n" /* roughness mapped to a more linear change in the roughness (proposed by [2]) */
		"    vec3 diffuse_color;\n"       /* color contribution from diffuse lighting */
		"    vec3 specular_color;\n"      /* color contribution from specular lighting */
		"};\n");

	str_cat(&shader_buffer,
		"vec3 diffuse(PBRInfo pbrInputs)\n"
		"{\n"
		"    return pbrInputs.diffuse_color / M_PI;\n"
		"}\n"
		"vec3 specularReflection(PBRInfo pbrInputs)\n"
		"{\n"
		"    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);\n"
		"}\n");

	str_cat(&shader_buffer,
		"float geometricOcclusion(PBRInfo pbrInputs)\n"
		"{\n"
		"    float NdotL = pbrInputs.NdotL;\n"
		"    float NdotV = pbrInputs.NdotV;\n"
		"    float r = pbrInputs.alpha_roughness_sq;\n"
		"    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));\n"
		"    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));\n"
		"    return attenuationL * attenuationV;\n"
		"}\n");
	str_cat(&shader_buffer,
		"float microfacetDistribution(PBRInfo pbrInputs)\n"
		"{\n"
		"    float roughnessSq = pbrInputs.alpha_roughness_sq;\n"
		"    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;\n"
		"    return roughnessSq / (M_PI * f * f);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec4 pbr(vec4 base_color, vec2 metallic_roughness,\n"
		"		vec3 light_color, vec3 light_dir, vec3 c_pos,\n"
		"		vec3 c_nor)\n"
		"{\n"
		"	/* Metallic and Roughness material properties are packed together\n"
		"	 * In glTF, these factors can be specified by fixed scalar values\n"
		"	 * or from a metallic-roughness map */\n"
		"	float perceptualRoughness = metallic_roughness.y;\n"
		"	float metallic = metallic_roughness.x;\n"
		"	perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);\n"
		"	metallic = clamp(metallic, 0.0, 1.0);\n");

	str_cat(&shader_buffer,
		"	/* Roughness is authored as perceptual roughness; as is convention,\n"
		"	 * convert to material roughness by squaring the perceptual roughness. */\n"
		"	float alpha_roughness_sq = perceptualRoughness * perceptualRoughness;\n"
		"	alpha_roughness_sq = alpha_roughness_sq * alpha_roughness_sq;\n"
		"	vec3 f0 = vec3(0.04);\n"
		"	vec3 diffuse_color = base_color.rgb * (vec3(1.0) - f0);\n"
		"	diffuse_color *= 1.0 - metallic;\n"
		"	vec3 specular_color = mix(f0, base_color.rgb, metallic);\n");

	str_cat(&shader_buffer,
		"	float reflectance = max(max(specular_color.r, specular_color.g), specular_color.b);\n"
			/* For typical incident reflectance range (between 4% to 100%) set
			 * the grazing reflectance to 100% for typical fresnel effect. */
			/* For very low reflectance range on highly diffuse
			 * objects (below 4%), incrementally reduce grazing reflecance to 0%. */
		"	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);\n"
		"	vec3 specularEnvironmentR0 = specular_color.rgb;\n"
		"	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;\n");

	str_cat(&shader_buffer,
		"	vec3 v = normalize(-c_pos);    /* Vector from surface point to camera */\n"
		"	vec3 l = normalize(light_dir); /* Vector from surface point to light */\n"
		"	vec3 h = normalize(l+v);       /* Half vector between both l and v */\n");

	str_cat(&shader_buffer,
		"	float NdotL = clamp(dot(c_nor, l), 0.001, 1.0);\n"
		"	float NdotV = clamp(abs(dot(c_nor, v)), 0.001, 1.0);\n"
		"	float NdotH = clamp(dot(c_nor, h), 0.0, 1.0);\n"
		"	float LdotH = clamp(dot(l, h), 0.0, 1.0);\n"
		"	float VdotH = clamp(dot(v, h), 0.0, 1.0);\n");

	str_cat(&shader_buffer,
		"	PBRInfo pbrInputs = PBRInfo( NdotL, NdotV, NdotH, LdotH, VdotH,\n"
		"			perceptualRoughness, metallic, specularEnvironmentR0,\n"
		"			specularEnvironmentR90, alpha_roughness_sq, diffuse_color,\n"
		"			specular_color);\n"
		"	/* Calculate the shading terms for the microfacet specular shading model */\n"
		"	vec3  F = specularReflection(pbrInputs);\n"
		"	float G = geometricOcclusion(pbrInputs);\n"
		"	float D = microfacetDistribution(pbrInputs);\n");

	str_cat(&shader_buffer,
		"	/* Calculation of analytical lighting contribution */\n"
		"	vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);\n"
		"	vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);\n"
		"	/* Obtain final intensity as reflectance (BRDF) scaled by the\n"
		"	 * energy of the light (cosine law) */\n"
		"	vec3 color = NdotL * light_color * (diffuseContrib + specContrib);\n"
		"	return vec4(pow(color,vec3(1.0/2.2)), base_color.a);\n"
		"}\n");

	str_cat(&shader_buffer,
		"#endif\n");

	shader_add_source("candle:common.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_volum()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D depth;\n"
		"	sampler2D albedo;\n"
		"} gbuffer;\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	if (light(volumetric) == 0.0) discard;\n"
		"	float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;\n"
		"	if(gl_FragCoord.z < depth)\n"
		"	{\n"
		"		depth = gl_FragCoord.z;\n"
		"	}\n"
		"	float intensity = abs(light(volumetric));\n"
		"	bool inverted = light(volumetric) < 0.0;\n");

	str_cat(&shader_buffer,
		"	vec3 c_pos = get_position(depth,  pixel_pos());\n"
		"	vec3 w_cam = camera(pos);\n"
		"	vec3 w_pos = (camera(model) * vec4(c_pos, 1.0)).xyz;\n"
		"	float color = 0.0;\n"
		"	int iterations = 32;\n"
		"	vec3 norm_dir = normalize(w_cam - w_pos);\n"
		"	vec3 dir = norm_dir * ((2.0 * light(radius)) / float(iterations));\n"
		"	w_pos += dir * dither_value();\n"
		"	bool passed = false;\n");

	str_cat(&shader_buffer,
		"	for (int i = 0; i < iterations; i++)\n"
		"	{\n"
		"		w_pos += dir;\n"
		"		vec3 dif = w_pos - obj_pos;\n"
		"		float len = length(dif);\n"
		"		float l = len / light(radius);\n"
		"		float attenuation = ((3.0 - l*3.0) / (1.0+1.3*(pow(l*3.0-0.1, 2.0))))/3.0;\n"
		"		attenuation = clamp(attenuation, 0.0, 1.0);\n"
		"		if (attenuation > 0.0) {\n"
		"			passed = true;\n"
		"			float z;\n"
		"			float Z = lookup_single(dif, z);\n"
		"			bool in_light = Z >= z;\n");
	str_cat(&shader_buffer,
		"			in_light = inverted ? !in_light : in_light;\n"
		"			color += in_light ? attenuation * (1.0f - dot(norm_dir, dif / len)) : 0.0;\n"
		"		}\n"
		"		else if (passed)\n"
		"		{\n"
		"			break;\n"
		"		}\n"
		"	}\n"
		"	FragColor = vec4(light(color).rgb * (color / float(iterations)) * intensity, 1.);\n"
		"}\n");

	shader_add_source("candle:volum.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_gaussian()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"layout (location = 0) out vec4 FragColor;\n"
		"uniform bool horizontal;\n"
		"uniform float weight[6] = float[] (0.382925, 0.24173, 0.060598, 0.005977, 0.000229, 0.000003);\n"
		"BUFFER {\n"
		"	sampler2D color;\n"
		"} buf;\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	ivec2 tc = ivec2(gl_FragCoord);\n"
		"	vec3 c = texelFetch(buf.color, pp, 0).rgb * weight[0];\n"
		"	if(horizontal)\n"
		"	{\n"
		"		for(int i = 1; i < 6; ++i)\n"
		"		{\n"
		"			ivec2 off = ivec2(i, 0.0);\n"
		"			c += texelFetch(buf.color, pp + off, 0).rgb * weight[i];\n"
		"			c += texelFetch(buf.color, pp - off, 0).rgb * weight[i];\n"
		"		}\n"
		"	}\n");
	str_cat(&shader_buffer,
		"	else\n"
		"	{\n"
		"		for(int i = 1; i < 6; ++i)\n"
		"		{\n"
		"			ivec2 off = ivec2(0.0, * i);\n"
		"			c += texelFetch(buf.color, pp + off, 0).rgb * weight[i];\n"
		"			c += texelFetch(buf.color, pp - off, 0).rgb * weight[i];\n"
		"		}\n"
		"	}\n"
		"	FragColor = vec4(c, 0.4);\n"
		"}\n");

	shader_add_source("candle:gaussian.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_marching()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"uniform sampler3D values;\n"
		"/* uniform isampler2D triTableTex; */\n"
		"/* uniform float iso_level; */\n"
		"float iso_level = 0.9;\n"
		"float grid_size = 0.2;\n");

	str_cat(&shader_buffer,
		"const vec3 vert_offsets[8] = vec3[](\n"
		"	vec3(0.0, 0.0, 0.0),\n"
		"	vec3(1.0, 0.0, 0.0),\n"
		"	vec3(1.0, 1.0, 0.0),\n"
		"	vec3(0.0, 1.0, 0.0),\n"
		"	vec3(0.0, 0.0, 1.0),\n"
		"	vec3(1.0, 0.0, 1.0),\n"
		"	vec3(1.0, 1.0, 1.0),\n"
		"	vec3(0.0, 1.0, 1.0)\n"
		");\n");

	str_cat(&shader_buffer,
		"#define b3(r,g,b) (ivec3(r, g, b))\n"
		"#define z3 ivec3(-1, -1, -1)\n"
		"ivec3 tri_table[256 * 5] = ivec3[](\n"
		"	z3,             z3,             z3,             z3,             z3,            \n"
		"	b3( 0,  8,  3), z3,             z3,             z3,             z3,            \n"
		"	b3( 0,  1,  9), z3,             z3,             z3,             z3,            \n"
		"	b3( 1,  8,  3), b3( 9,  8,  1), z3,             z3,             z3,            \n"
		"	b3( 1,  2, 10), z3,             z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  8,  3), b3( 1,  2, 10), z3,             z3,             z3,            \n"
		"	b3( 9,  2, 10), b3( 0,  2,  9), z3,             z3,             z3,            \n"
		"	b3( 2,  8,  3), b3( 2, 10,  8), b3(10,  9,  8), z3,             z3,            \n"
		"	b3( 3, 11,  2), z3,             z3,             z3,             z3,            \n"
		"	b3( 0, 11,  2), b3( 8, 11,  0), z3,             z3,             z3,            \n"
		"	b3( 1,  9,  0), b3( 2,  3, 11), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1, 11,  2), b3( 1,  9, 11), b3( 9,  8, 11), z3,             z3,            \n"
		"	b3( 3, 10,  1), b3(11, 10,  3), z3,             z3,             z3,            \n"
		"	b3( 0, 10,  1), b3( 0,  8, 10), b3( 8, 11, 10), z3,             z3,            \n"
		"	b3( 3,  9,  0), b3( 3, 11,  9), b3(11, 10,  9), z3,             z3,            \n"
		"	b3( 9,  8, 10), b3(10,  8, 11), z3,             z3,             z3,            \n"
		"	b3( 4,  7,  8), z3,             z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 4,  3,  0), b3( 7,  3,  4), z3,             z3,             z3,            \n"
		"	b3( 0,  1,  9), b3( 8,  4,  7), z3,             z3,             z3,            \n"
		"	b3( 4,  1,  9), b3( 4,  7,  1), b3( 7,  3,  1), z3,             z3,            \n"
		"	b3( 1,  2, 10), b3( 8,  4,  7), z3,             z3,             z3,            \n"
		"	b3( 3,  4,  7), b3( 3,  0,  4), b3( 1,  2, 10), z3,             z3,            \n"
		"	b3( 9,  2, 10), b3( 9,  0,  2), b3( 8,  4,  7), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 2, 10,  9), b3( 2,  9,  7), b3( 2,  7,  3), b3( 7,  9,  4), z3,            \n"
		"	b3( 8,  4,  7), b3( 3, 11,  2), z3,             z3,             z3,            \n"
		"	b3(11,  4,  7), b3(11,  2,  4), b3( 2,  0,  4), z3,             z3,            \n"
		"	b3( 9,  0,  1), b3( 8,  4,  7), b3( 2,  3, 11), z3,             z3,            \n"
		"	b3( 4,  7, 11), b3( 9,  4, 11), b3( 9, 11,  2), b3( 9,  2,  1), z3,            \n"
		"	b3( 3, 10,  1), b3( 3, 11, 10), b3( 7,  8,  4), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1, 11, 10), b3( 1,  4, 11), b3( 1,  0,  4), b3( 7, 11,  4), z3,            \n"
		"	b3( 4,  7,  8), b3( 9,  0, 11), b3( 9, 11, 10), b3(11,  0,  3), z3,            \n"
		"	b3( 4,  7, 11), b3( 4, 11,  9), b3( 9, 11, 10), z3,             z3,            \n"
		"	b3( 9,  5,  4), z3,             z3,             z3,             z3,            \n"
		"	b3( 9,  5,  4), b3( 0,  8,  3), z3,             z3,             z3,            \n"
		"	b3( 0,  5,  4), b3( 1,  5,  0), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 8,  5,  4), b3( 8,  3,  5), b3( 3,  1,  5), z3,             z3,            \n"
		"	b3( 1,  2, 10), b3( 9,  5,  4), z3,             z3,             z3,            \n"
		"	b3( 3,  0,  8), b3( 1,  2, 10), b3( 4,  9,  5), z3,             z3,            \n"
		"	b3( 5,  2, 10), b3( 5,  4,  2), b3( 4,  0,  2), z3,             z3,            \n"
		"	b3( 2, 10,  5), b3( 3,  2,  5), b3( 3,  5,  4), b3( 3,  4,  8), z3,            \n"
		"	b3( 9,  5,  4), b3( 2,  3, 11), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0, 11,  2), b3( 0,  8, 11), b3( 4,  9,  5), z3,             z3,            \n"
		"	b3( 0,  5,  4), b3( 0,  1,  5), b3( 2,  3, 11), z3,             z3,            \n"
		"	b3( 2,  1,  5), b3( 2,  5,  8), b3( 2,  8, 11), b3( 4,  8,  5), z3,            \n"
		"	b3(10,  3, 11), b3(10,  1,  3), b3( 9,  5,  4), z3,             z3,            \n"
		"	b3( 4,  9,  5), b3( 0,  8,  1), b3( 8, 10,  1), b3( 8, 11, 10), z3,            \n"
		"	b3( 5,  4,  0), b3( 5,  0, 11), b3( 5, 11, 10), b3(11,  0,  3), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 5,  4,  8), b3( 5,  8, 10), b3(10,  8, 11), z3,             z3,            \n"
		"	b3( 9,  7,  8), b3( 5,  7,  9), z3,             z3,             z3,            \n"
		"	b3( 9,  3,  0), b3( 9,  5,  3), b3( 5,  7,  3), z3,             z3,            \n"
		"	b3( 0,  7,  8), b3( 0,  1,  7), b3( 1,  5,  7), z3,             z3,            \n"
		"	b3( 1,  5,  3), b3( 3,  5,  7), z3,             z3,             z3,            \n"
		"	b3( 9,  7,  8), b3( 9,  5,  7), b3(10,  1,  2), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3(10,  1,  2), b3( 9,  5,  0), b3( 5,  3,  0), b3( 5,  7,  3), z3,            \n"
		"	b3( 8,  0,  2), b3( 8,  2,  5), b3( 8,  5,  7), b3(10,  5,  2), z3,            \n"
		"	b3( 2, 10,  5), b3( 2,  5,  3), b3( 3,  5,  7), z3,             z3,            \n"
		"	b3( 7,  9,  5), b3( 7,  8,  9), b3( 3, 11,  2), z3,             z3,            \n"
		"	b3( 9,  5,  7), b3( 9,  7,  2), b3( 9,  2,  0), b3( 2,  7, 11), z3,            \n"
		"	b3( 2,  3, 11), b3( 0,  1,  8), b3( 1,  7,  8), b3( 1,  5,  7), z3,            \n");
	str_cat(&shader_buffer,
		"	b3(11,  2,  1), b3(11,  1,  7), b3( 7,  1,  5), z3,             z3,            \n"
		"	b3( 9,  5,  8), b3( 8,  5,  7), b3(10,  1,  3), b3(10,  3, 11), z3,            \n"
		"	b3( 5,  7,  0), b3( 5,  0,  9), b3( 7, 11,  0), b3( 1,  0, 10), b3(11, 10,  0),\n"
		"	b3(11, 10,  0), b3(11,  0,  3), b3(10,  5,  0), b3( 8,  0,  7), b3( 5,  7,  0),\n"
		"	b3(11, 10,  5), b3( 7, 11,  5), z3,             z3,             z3,            \n"
		"	b3(10,  6,  5), z3,             z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  8,  3), b3( 5, 10,  6), z3,             z3,             z3,            \n"
		"	b3( 9,  0,  1), b3( 5, 10,  6), z3,             z3,             z3,            \n"
		"	b3( 1,  8,  3), b3( 1,  9,  8), b3( 5, 10,  6), z3,             z3,            \n"
		"	b3( 1,  6,  5), b3( 2,  6,  1), z3,             z3,             z3,            \n"
		"	b3( 1,  6,  5), b3( 1,  2,  6), b3( 3,  0,  8), z3,             z3,            \n"
		"	b3( 9,  6,  5), b3( 9,  0,  6), b3( 0,  2,  6), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 5,  9,  8), b3( 5,  8,  2), b3( 5,  2,  6), b3( 3,  2,  8), z3,            \n"
		"	b3( 2,  3, 11), b3(10,  6,  5), z3,             z3,             z3,            \n"
		"	b3(11,  0,  8), b3(11,  2,  0), b3(10,  6,  5), z3,             z3,            \n"
		"	b3( 0,  1,  9), b3( 2,  3, 11), b3( 5, 10,  6), z3,             z3,            \n"
		"	b3( 5, 10,  6), b3( 1,  9,  2), b3( 9, 11,  2), b3( 9,  8, 11), z3,            \n"
		"	b3( 6,  3, 11), b3( 6,  5,  3), b3( 5,  1,  3), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  8, 11), b3( 0, 11,  5), b3( 0,  5,  1), b3( 5, 11,  6), z3,            \n"
		"	b3( 3, 11,  6), b3( 0,  3,  6), b3( 0,  6,  5), b3( 0,  5,  9), z3,            \n"
		"	b3( 6,  5,  9), b3( 6,  9, 11), b3(11,  9,  8), z3,             z3,            \n"
		"	b3( 5, 10,  6), b3( 4,  7,  8), z3,             z3,             z3,            \n"
		"	b3( 4,  3,  0), b3( 4,  7,  3), b3( 6,  5, 10), z3,             z3,            \n"
		"	b3( 1,  9,  0), b3( 5, 10,  6), b3( 8,  4,  7), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3(10,  6,  5), b3( 1,  9,  7), b3( 1,  7,  3), b3( 7,  9,  4), z3,            \n"
		"	b3( 6,  1,  2), b3( 6,  5,  1), b3( 4,  7,  8), z3,             z3,            \n"
		"	b3( 1,  2,  5), b3( 5,  2,  6), b3( 3,  0,  4), b3( 3,  4,  7), z3,            \n"
		"	b3( 8,  4,  7), b3( 9,  0,  5), b3( 0,  6,  5), b3( 0,  2,  6), z3,            \n"
		"	b3( 7,  3,  9), b3( 7,  9,  4), b3( 3,  2,  9), b3( 5,  9,  6), b3( 2,  6,  9),\n"
		"	b3( 3, 11,  2), b3( 7,  8,  4), b3(10,  6,  5), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 5, 10,  6), b3( 4,  7,  2), b3( 4,  2,  0), b3( 2,  7, 11), z3,            \n"
		"	b3( 0,  1,  9), b3( 4,  7,  8), b3( 2,  3, 11), b3( 5, 10,  6), z3,            \n"
		"	b3( 9,  2,  1), b3( 9, 11,  2), b3( 9,  4, 11), b3( 7, 11,  4), b3( 5, 10,  6),\n"
		"	b3( 8,  4,  7), b3( 3, 11,  5), b3( 3,  5,  1), b3( 5, 11,  6), z3,            \n"
		"	b3( 5,  1, 11), b3( 5, 11,  6), b3( 1,  0, 11), b3( 7, 11,  4), b3( 0,  4, 11),\n"
		"	b3( 0,  5,  9), b3( 0,  6,  5), b3( 0,  3,  6), b3(11,  6,  3), b3( 8,  4,  7),\n");
	str_cat(&shader_buffer,
		"	b3( 6,  5,  9), b3( 6,  9, 11), b3( 4,  7,  9), b3( 7, 11,  9), z3,            \n"
		"	b3(10,  4,  9), b3( 6,  4, 10), z3,             z3,             z3,            \n"
		"	b3( 4, 10,  6), b3( 4,  9, 10), b3( 0,  8,  3), z3,             z3,            \n"
		"	b3(10,  0,  1), b3(10,  6,  0), b3( 6,  4,  0), z3,             z3,            \n"
		"	b3( 8,  3,  1), b3( 8,  1,  6), b3( 8,  6,  4), b3( 6,  1, 10), z3,            \n"
		"	b3( 1,  4,  9), b3( 1,  2,  4), b3( 2,  6,  4), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 3,  0,  8), b3( 1,  2,  9), b3( 2,  4,  9), b3( 2,  6,  4), z3,            \n"
		"	b3( 0,  2,  4), b3( 4,  2,  6), z3,             z3,             z3,            \n"
		"	b3( 8,  3,  2), b3( 8,  2,  4), b3( 4,  2,  6), z3,             z3,            \n"
		"	b3(10,  4,  9), b3(10,  6,  4), b3(11,  2,  3), z3,             z3,            \n"
		"	b3( 0,  8,  2), b3( 2,  8, 11), b3( 4,  9, 10), b3( 4, 10,  6), z3,            \n"
		"	b3( 3, 11,  2), b3( 0,  1,  6), b3( 0,  6,  4), b3( 6,  1, 10), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 6,  4,  1), b3( 6,  1, 10), b3( 4,  8,  1), b3( 2,  1, 11), b3( 8, 11,  1),\n"
		"	b3( 9,  6,  4), b3( 9,  3,  6), b3( 9,  1,  3), b3(11,  6,  3), z3,            \n"
		"	b3( 8, 11,  1), b3( 8,  1,  0), b3(11,  6,  1), b3( 9,  1,  4), b3( 6,  4,  1),\n"
		"	b3( 3, 11,  6), b3( 3,  6,  0), b3( 0,  6,  4), z3,             z3,            \n"
		"	b3( 6,  4,  8), b3(11,  6,  8), z3,             z3,             z3,            \n"
		"	b3( 7, 10,  6), b3( 7,  8, 10), b3( 8,  9, 10), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  7,  3), b3( 0, 10,  7), b3( 0,  9, 10), b3( 6,  7, 10), z3,            \n"
		"	b3(10,  6,  7), b3( 1, 10,  7), b3( 1,  7,  8), b3( 1,  8,  0), z3,            \n"
		"	b3(10,  6,  7), b3(10,  7,  1), b3( 1,  7,  3), z3,             z3,            \n"
		"	b3( 1,  2,  6), b3( 1,  6,  8), b3( 1,  8,  9), b3( 8,  6,  7), z3,            \n"
		"	b3( 2,  6,  9), b3( 2,  9,  1), b3( 6,  7,  9), b3( 0,  9,  3), b3( 7,  3,  9),\n"
		"	b3( 7,  8,  0), b3( 7,  0,  6), b3( 6,  0,  2), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 7,  3,  2), b3( 6,  7,  2), z3,             z3,             z3,            \n"
		"	b3( 2,  3, 11), b3(10,  6,  8), b3(10,  8,  9), b3( 8,  6,  7), z3,            \n"
		"	b3( 2,  0,  7), b3( 2,  7, 11), b3( 0,  9,  7), b3( 6,  7, 10), b3( 9, 10,  7),\n"
		"	b3( 1,  8,  0), b3( 1,  7,  8), b3( 1, 10,  7), b3( 6,  7, 10), b3( 2,  3, 11),\n"
		"	b3(11,  2,  1), b3(11,  1,  7), b3(10,  6,  1), b3( 6,  7,  1), z3,            \n"
		"	b3( 8,  9,  6), b3( 8,  6,  7), b3( 9,  1,  6), b3(11,  6,  3), b3( 1,  3,  6),\n");
	str_cat(&shader_buffer,
		"	b3( 0,  9,  1), b3(11,  6,  7), z3,             z3,             z3,            \n"
		"	b3( 7,  8,  0), b3( 7,  0,  6), b3( 3, 11,  0), b3(11,  6,  0), z3,            \n"
		"	b3( 7, 11,  6), z3,             z3,             z3,             z3,            \n"
		"	b3( 7,  6, 11), z3,             z3,             z3,             z3,            \n"
		"	b3( 3,  0,  8), b3(11,  7,  6), z3,             z3,             z3,            \n"
		"	b3( 0,  1,  9), b3(11,  7,  6), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 8,  1,  9), b3( 8,  3,  1), b3(11,  7,  6), z3,             z3,            \n"
		"	b3(10,  1,  2), b3( 6, 11,  7), z3,             z3,             z3,            \n"
		"	b3( 1,  2, 10), b3( 3,  0,  8), b3( 6, 11,  7), z3,             z3,            \n"
		"	b3( 2,  9,  0), b3( 2, 10,  9), b3( 6, 11,  7), z3,             z3,            \n"
		"	b3( 6, 11,  7), b3( 2, 10,  3), b3(10,  8,  3), b3(10,  9,  8), z3,            \n"
		"	b3( 7,  2,  3), b3( 6,  2,  7), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 7,  0,  8), b3( 7,  6,  0), b3( 6,  2,  0), z3,             z3,            \n"
		"	b3( 2,  7,  6), b3( 2,  3,  7), b3( 0,  1,  9), z3,             z3,            \n"
		"	b3( 1,  6,  2), b3( 1,  8,  6), b3( 1,  9,  8), b3( 8,  7,  6), z3,            \n"
		"	b3(10,  7,  6), b3(10,  1,  7), b3( 1,  3,  7), z3,             z3,            \n"
		"	b3(10,  7,  6), b3( 1,  7, 10), b3( 1,  8,  7), b3( 1,  0,  8), z3,            \n"
		"	b3( 0,  3,  7), b3( 0,  7, 10), b3( 0, 10,  9), b3( 6, 10,  7), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 7,  6, 10), b3( 7, 10,  8), b3( 8, 10,  9), z3,             z3,            \n"
		"	b3( 6,  8,  4), b3(11,  8,  6), z3,             z3,             z3,            \n"
		"	b3( 3,  6, 11), b3( 3,  0,  6), b3( 0,  4,  6), z3,             z3,            \n"
		"	b3( 8,  6, 11), b3( 8,  4,  6), b3( 9,  0,  1), z3,             z3,            \n"
		"	b3( 9,  4,  6), b3( 9,  6,  3), b3( 9,  3,  1), b3(11,  3,  6), z3,            \n"
		"	b3( 6,  8,  4), b3( 6, 11,  8), b3( 2, 10,  1), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1,  2, 10), b3( 3,  0, 11), b3( 0,  6, 11), b3( 0,  4,  6), z3,            \n"
		"	b3( 4, 11,  8), b3( 4,  6, 11), b3( 0,  2,  9), b3( 2, 10,  9), z3,            \n"
		"	b3(10,  9,  3), b3(10,  3,  2), b3( 9,  4,  3), b3(11,  3,  6), b3( 4,  6,  3),\n"
		"	b3( 8,  2,  3), b3( 8,  4,  2), b3( 4,  6,  2), z3,             z3,            \n"
		"	b3( 0,  4,  2), b3( 4,  6,  2), z3,             z3,             z3,            \n"
		"	b3( 1,  9,  0), b3( 2,  3,  4), b3( 2,  4,  6), b3( 4,  3,  8), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1,  9,  4), b3( 1,  4,  2), b3( 2,  4,  6), z3,             z3,            \n"
		"	b3( 8,  1,  3), b3( 8,  6,  1), b3( 8,  4,  6), b3( 6, 10,  1), z3,            \n"
		"	b3(10,  1,  0), b3(10,  0,  6), b3( 6,  0,  4), z3,             z3,            \n"
		"	b3( 4,  6,  3), b3( 4,  3,  8), b3( 6, 10,  3), b3( 0,  3,  9), b3(10,  9,  3),\n"
		"	b3(10,  9,  4), b3( 6, 10,  4), z3,             z3,             z3,            \n"
		"	b3( 4,  9,  5), b3( 7,  6, 11), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  8,  3), b3( 4,  9,  5), b3(11,  7,  6), z3,             z3,            \n"
		"	b3( 5,  0,  1), b3( 5,  4,  0), b3( 7,  6, 11), z3,             z3,            \n"
		"	b3(11,  7,  6), b3( 8,  3,  4), b3( 3,  5,  4), b3( 3,  1,  5), z3,            \n"
		"	b3( 9,  5,  4), b3(10,  1,  2), b3( 7,  6, 11), z3,             z3,            \n"
		"	b3( 6, 11,  7), b3( 1,  2, 10), b3( 0,  8,  3), b3( 4,  9,  5), z3,            \n"
		"	b3( 7,  6, 11), b3( 5,  4, 10), b3( 4,  2, 10), b3( 4,  0,  2), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 3,  4,  8), b3( 3,  5,  4), b3( 3,  2,  5), b3(10,  5,  2), b3(11,  7,  6),\n"
		"	b3( 7,  2,  3), b3( 7,  6,  2), b3( 5,  4,  9), z3,             z3,            \n"
		"	b3( 9,  5,  4), b3( 0,  8,  6), b3( 0,  6,  2), b3( 6,  8,  7), z3,            \n"
		"	b3( 3,  6,  2), b3( 3,  7,  6), b3( 1,  5,  0), b3( 5,  4,  0), z3,            \n"
		"	b3( 6,  2,  8), b3( 6,  8,  7), b3( 2,  1,  8), b3( 4,  8,  5), b3( 1,  5,  8),\n"
		"	b3( 9,  5,  4), b3(10,  1,  6), b3( 1,  7,  6), b3( 1,  3,  7), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1,  6, 10), b3( 1,  7,  6), b3( 1,  0,  7), b3( 8,  7,  0), b3( 9,  5,  4),\n"
		"	b3( 4,  0, 10), b3( 4, 10,  5), b3( 0,  3, 10), b3( 6, 10,  7), b3( 3,  7, 10),\n"
		"	b3( 7,  6, 10), b3( 7, 10,  8), b3( 5,  4, 10), b3( 4,  8, 10), z3,            \n"
		"	b3( 6,  9,  5), b3( 6, 11,  9), b3(11,  8,  9), z3,             z3,            \n"
		"	b3( 3,  6, 11), b3( 0,  6,  3), b3( 0,  5,  6), b3( 0,  9,  5), z3,            \n"
		"	b3( 0, 11,  8), b3( 0,  5, 11), b3( 0,  1,  5), b3( 5,  6, 11), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 6, 11,  3), b3( 6,  3,  5), b3( 5,  3,  1), z3,             z3,            \n"
		"	b3( 1,  2, 10), b3( 9,  5, 11), b3( 9, 11,  8), b3(11,  5,  6), z3,            \n"
		"	b3( 0, 11,  3), b3( 0,  6, 11), b3( 0,  9,  6), b3( 5,  6,  9), b3( 1,  2, 10),\n"
		"	b3(11,  8,  5), b3(11,  5,  6), b3( 8,  0,  5), b3(10,  5,  2), b3( 0,  2,  5),\n"
		"	b3( 6, 11,  3), b3( 6,  3,  5), b3( 2, 10,  3), b3(10,  5,  3), z3,            \n"
		"	b3( 5,  8,  9), b3( 5,  2,  8), b3( 5,  6,  2), b3( 3,  8,  2), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 9,  5,  6), b3( 9,  6,  0), b3( 0,  6,  2), z3,             z3,            \n"
		"	b3( 1,  5,  8), b3( 1,  8,  0), b3( 5,  6,  8), b3( 3,  8,  2), b3( 6,  2,  8),\n"
		"	b3( 1,  5,  6), b3( 2,  1,  6), z3,             z3,             z3,            \n"
		"	b3( 1,  3,  6), b3( 1,  6, 10), b3( 3,  8,  6), b3( 5,  6,  9), b3( 8,  9,  6),\n"
		"	b3(10,  1,  0), b3(10,  0,  6), b3( 9,  5,  0), b3( 5,  6,  0), z3,            \n"
		"	b3( 0,  3,  8), b3( 5,  6, 10), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3(10,  5,  6), z3,             z3,             z3,             z3,            \n"
		"	b3(11,  5, 10), b3( 7,  5, 11), z3,             z3,             z3,            \n"
		"	b3(11,  5, 10), b3(11,  7,  5), b3( 8,  3,  0), z3,             z3,            \n"
		"	b3( 5, 11,  7), b3( 5, 10, 11), b3( 1,  9,  0), z3,             z3,            \n"
		"	b3(10,  7,  5), b3(10, 11,  7), b3( 9,  8,  1), b3( 8,  3,  1), z3,            \n"
		"	b3(11,  1,  2), b3(11,  7,  1), b3( 7,  5,  1), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  8,  3), b3( 1,  2,  7), b3( 1,  7,  5), b3( 7,  2, 11), z3,            \n"
		"	b3( 9,  7,  5), b3( 9,  2,  7), b3( 9,  0,  2), b3( 2, 11,  7), z3,            \n"
		"	b3( 7,  5,  2), b3( 7,  2, 11), b3( 5,  9,  2), b3( 3,  2,  8), b3( 9,  8,  2),\n"
		"	b3( 2,  5, 10), b3( 2,  3,  5), b3( 3,  7,  5), z3,             z3,            \n"
		"	b3( 8,  2,  0), b3( 8,  5,  2), b3( 8,  7,  5), b3(10,  2,  5), z3,            \n"
		"	b3( 9,  0,  1), b3( 5, 10,  3), b3( 5,  3,  7), b3( 3, 10,  2), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 9,  8,  2), b3( 9,  2,  1), b3( 8,  7,  2), b3(10,  2,  5), b3( 7,  5,  2),\n"
		"	b3( 1,  3,  5), b3( 3,  7,  5), z3,             z3,             z3,            \n"
		"	b3( 0,  8,  7), b3( 0,  7,  1), b3( 1,  7,  5), z3,             z3,            \n"
		"	b3( 9,  0,  3), b3( 9,  3,  5), b3( 5,  3,  7), z3,             z3,            \n"
		"	b3( 9,  8,  7), b3( 5,  9,  7), z3,             z3,             z3,            \n"
		"	b3( 5,  8,  4), b3( 5, 10,  8), b3(10, 11,  8), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 5,  0,  4), b3( 5, 11,  0), b3( 5, 10, 11), b3(11,  3,  0), z3,            \n"
		"	b3( 0,  1,  9), b3( 8,  4, 10), b3( 8, 10, 11), b3(10,  4,  5), z3,            \n"
		"	b3(10, 11,  4), b3(10,  4,  5), b3(11,  3,  4), b3( 9,  4,  1), b3( 3,  1,  4),\n"
		"	b3( 2,  5,  1), b3( 2,  8,  5), b3( 2, 11,  8), b3( 4,  5,  8), z3,            \n"
		"	b3( 0,  4, 11), b3( 0, 11,  3), b3( 4,  5, 11), b3( 2, 11,  1), b3( 5,  1, 11),\n"
		"	b3( 0,  2,  5), b3( 0,  5,  9), b3( 2, 11,  5), b3( 4,  5,  8), b3(11,  8,  5),\n");
	str_cat(&shader_buffer,
		"	b3( 9,  4,  5), b3( 2, 11,  3), z3,             z3,             z3,            \n"
		"	b3( 2,  5, 10), b3( 3,  5,  2), b3( 3,  4,  5), b3( 3,  8,  4), z3,            \n"
		"	b3( 5, 10,  2), b3( 5,  2,  4), b3( 4,  2,  0), z3,             z3,            \n"
		"	b3( 3, 10,  2), b3( 3,  5, 10), b3( 3,  8,  5), b3( 4,  5,  8), b3( 0,  1,  9),\n"
		"	b3( 5, 10,  2), b3( 5,  2,  4), b3( 1,  9,  2), b3( 9,  4,  2), z3,            \n"
		"	b3( 8,  4,  5), b3( 8,  5,  3), b3( 3,  5,  1), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 0,  4,  5), b3( 1,  0,  5), z3,             z3,             z3,            \n"
		"	b3( 8,  4,  5), b3( 8,  5,  3), b3( 9,  0,  5), b3( 0,  3,  5), z3,            \n"
		"	b3( 9,  4,  5), z3,             z3,             z3,             z3,            \n"
		"	b3( 4, 11,  7), b3( 4,  9, 11), b3( 9, 10, 11), z3,             z3,            \n"
		"	b3( 0,  8,  3), b3( 4,  9,  7), b3( 9, 11,  7), b3( 9, 10, 11), z3,            \n"
		"	b3( 1, 10, 11), b3( 1, 11,  4), b3( 1,  4,  0), b3( 7,  4, 11), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 3,  1,  4), b3( 3,  4,  8), b3( 1, 10,  4), b3( 7,  4, 11), b3(10, 11,  4),\n"
		"	b3( 4, 11,  7), b3( 9, 11,  4), b3( 9,  2, 11), b3( 9,  1,  2), z3,            \n"
		"	b3( 9,  7,  4), b3( 9, 11,  7), b3( 9,  1, 11), b3( 2, 11,  1), b3( 0,  8,  3),\n"
		"	b3(11,  7,  4), b3(11,  4,  2), b3( 2,  4,  0), z3,             z3,            \n"
		"	b3(11,  7,  4), b3(11,  4,  2), b3( 8,  3,  4), b3( 3,  2,  4), z3,            \n"
		"	b3( 2,  9, 10), b3( 2,  7,  9), b3( 2,  3,  7), b3( 7,  4,  9), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 9, 10,  7), b3( 9,  7,  4), b3(10,  2,  7), b3( 8,  7,  0), b3( 2,  0,  7),\n"
		"	b3( 3,  7, 10), b3( 3, 10,  2), b3( 7,  4, 10), b3( 1, 10,  0), b3( 4,  0, 10),\n"
		"	b3( 1, 10,  2), b3( 8,  7,  4), z3,             z3,             z3,            \n"
		"	b3( 4,  9,  1), b3( 4,  1,  7), b3( 7,  1,  3), z3,             z3,            \n"
		"	b3( 4,  9,  1), b3( 4,  1,  7), b3( 0,  8,  1), b3( 8,  7,  1), z3,            \n"
		"	b3( 4,  0,  3), b3( 7,  4,  3), z3,             z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 4,  8,  7), z3,             z3,             z3,             z3,            \n"
		"	b3( 9, 10,  8), b3(10, 11,  8), z3,             z3,             z3,            \n"
		"	b3( 3,  0,  9), b3( 3,  9, 11), b3(11,  9, 10), z3,             z3,            \n"
		"	b3( 0,  1, 10), b3( 0, 10,  8), b3( 8, 10, 11), z3,             z3,            \n"
		"	b3( 3,  1, 10), b3(11,  3, 10), z3,             z3,             z3,            \n"
		"	b3( 1,  2, 11), b3( 1, 11,  9), b3( 9, 11,  8), z3,             z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 3,  0,  9), b3( 3,  9, 11), b3( 1,  2,  9), b3( 2, 11,  9), z3,            \n"
		"	b3( 0,  2, 11), b3( 8,  0, 11), z3,             z3,             z3,            \n"
		"	b3( 3,  2, 11), z3,             z3,             z3,             z3,            \n"
		"	b3( 2,  3,  8), b3( 2,  8, 10), b3(10,  8,  9), z3,             z3,            \n"
		"	b3( 9, 10,  2), b3( 0,  9,  2), z3,             z3,             z3,            \n"
		"	b3( 2,  3,  8), b3( 2,  8, 10), b3( 0,  1,  8), b3( 1, 10,  8), z3,            \n");
	str_cat(&shader_buffer,
		"	b3( 1, 10,  2), z3,             z3,             z3,             z3,            \n"
		"	b3( 1,  3,  8), b3( 9,  1,  8), z3,             z3,             z3,            \n"
		"	b3( 0,  9,  1), z3,             z3,             z3,             z3,            \n"
		"	b3( 0,  3,  8), z3,             z3,             z3,             z3,            \n"
		"	z3,             z3,             z3,             z3,             z3             \n"
		");\n");

	str_cat(&shader_buffer,
		"vec3 get_cubeColor(vec3 p)\n"
		"{\n"
		"	float t = scene.time * 3.;\n"
		"	float r0 = abs(cos(t * .3)) * 2. + .6;\n"
		"	float r1 = abs(sin(t * .35)) * 2. + .6;\n"
		"	vec3 origin = vec3(0., 2.5, 0.);\n"
		"	vec3 p1 = vec3(sin(t), cos(t), sin(t * .612)) * r0;\n"
		"	vec3 p2 = vec3(sin(t * 1.1), cos(t * 1.2), sin(t * .512)) * r1;\n");
	str_cat(&shader_buffer,
		"	float val = 1. / (dot(p - origin, p - origin) + 1.);\n"
		"	float val2 = .6 / (dot(p - origin - p1, p - origin - p1) + 0.7);\n"
		"	float val3 = .5 / (dot(p - origin - p2, p - origin - p2) + 0.9);\n"
		"	return vec3(val, val2, val3);\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 get_cubePos(int i)\n"
		"{\n"
		"	return gl_in[0].gl_Position.xyz + vert_offsets[i] * grid_size;\n"
		"}\n"
		"float get_cubeVal(vec3 p)\n"
		"{\n"
		"	vec3 color = get_cubeColor(p);\n"
		"	return color.x + color.y + color.z;\n"
		"}\n");

	str_cat(&shader_buffer,
		"vec3 isonormal(vec3 pos)\n"
		"{\n"
		"	vec3 nor;\n"
		"	float offset = grid_size * 0.5;\n"
		"	nor.x = get_cubeVal(pos + vec3(grid_size, 0.0, 0.0))\n"
		"	      - get_cubeVal(pos - vec3(grid_size, 0.0, 0.0));\n"
		"	nor.y = get_cubeVal(pos + vec3(0.0, grid_size, 0.0))\n"
		"	      - get_cubeVal(pos - vec3(0.0, grid_size, 0.0));\n"
		"	nor.z = get_cubeVal(pos + vec3(0.0, 0.0, grid_size))\n"
		"	      - get_cubeVal(pos - vec3(0.0, 0.0, grid_size));\n"
		"	return -normalize(nor);\n"
		"}\n");

	str_cat(&shader_buffer,
		"ivec3 triTableValue(int i, int j)\n"
		"{\n"
		"	return tri_table[i * 5 + j];\n"
		"	/* return texelFetch(triTableTex, ivec2(j, i), 0).a; */\n"
		"}\n"
		"vec3 vert_lerp(float iso_level, vec3 v0, float l0, vec3 v1, float l1)\n"
		"{\n"
		"	return mix(v0, v1, (iso_level - l0) / (l1 - l0));\n"
		"}\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	vec4 pos;\n"
		"	id = $id[0];\n"
		"	matid = $matid[0];\n"
		"	object_id = $object_id[0];\n"
		"	poly_id = $poly_id[0];\n"
		"	obj_pos = $obj_pos[0];\n"
		"	model = $model[0];\n"
		"	int cubeindex = 0;\n"
		"	vec3 cubesPos[8];\n");

	str_cat(&shader_buffer,
		"	cubesPos[0] = get_cubePos(0);\n"
		"	cubesPos[1] = get_cubePos(1);\n"
		"	cubesPos[2] = get_cubePos(2);\n"
		"	cubesPos[3] = get_cubePos(3);\n"
		"	cubesPos[4] = get_cubePos(4);\n"
		"	cubesPos[5] = get_cubePos(5);\n"
		"	cubesPos[6] = get_cubePos(6);\n"
		"	cubesPos[7] = get_cubePos(7);\n");

	str_cat(&shader_buffer,
		"	float cubeVal0 = get_cubeVal(cubesPos[0]);\n"
		"	float cubeVal1 = get_cubeVal(cubesPos[1]);\n"
		"	float cubeVal2 = get_cubeVal(cubesPos[2]);\n"
		"	float cubeVal3 = get_cubeVal(cubesPos[3]);\n"
		"	float cubeVal4 = get_cubeVal(cubesPos[4]);\n"
		"	float cubeVal5 = get_cubeVal(cubesPos[5]);\n"
		"	float cubeVal6 = get_cubeVal(cubesPos[6]);\n"
		"	float cubeVal7 = get_cubeVal(cubesPos[7]);\n");

	/* Determine the index into the edge table which */
	/* tells us which vertices are inside of the surface */
	str_cat(&shader_buffer,
		"	cubeindex = int(cubeVal0 < iso_level);\n"
		"	cubeindex += int(cubeVal1 < iso_level)*2;\n"
		"	cubeindex += int(cubeVal2 < iso_level)*4;\n"
		"	cubeindex += int(cubeVal3 < iso_level)*8;\n"
		"	cubeindex += int(cubeVal4 < iso_level)*16;\n"
		"	cubeindex += int(cubeVal5 < iso_level)*32;\n"
		"	cubeindex += int(cubeVal6 < iso_level)*64;\n"
		"	cubeindex += int(cubeVal7 < iso_level)*128;\n"
		/* Cube is entirely in/out of the surface */
		"	if (cubeindex == 0)\n"
		"		return;\n"
		"	if (cubeindex == 255)\n"
		"		return;\n");

	/* Find the vertices where the surface intersects the cube */
	str_cat(&shader_buffer,
		"	vec3 vertlist[12];\n"
		"	vertlist[0] = vert_lerp(iso_level, cubesPos[0], cubeVal0, cubesPos[1], cubeVal1);\n"
		"	vertlist[1] = vert_lerp(iso_level, cubesPos[1], cubeVal1, cubesPos[2], cubeVal2);\n"
		"	vertlist[2] = vert_lerp(iso_level, cubesPos[2], cubeVal2, cubesPos[3], cubeVal3);\n"
		"	vertlist[3] = vert_lerp(iso_level, cubesPos[3], cubeVal3, cubesPos[0], cubeVal0);\n"
		"	vertlist[4] = vert_lerp(iso_level, cubesPos[4], cubeVal4, cubesPos[5], cubeVal5);\n");
	str_cat(&shader_buffer,
		"	vertlist[5] = vert_lerp(iso_level, cubesPos[5], cubeVal5, cubesPos[6], cubeVal6);\n"
		"	vertlist[6] = vert_lerp(iso_level, cubesPos[6], cubeVal6, cubesPos[7], cubeVal7);\n"
		"	vertlist[7] = vert_lerp(iso_level, cubesPos[7], cubeVal7, cubesPos[4], cubeVal4);\n"
		"	vertlist[8] = vert_lerp(iso_level, cubesPos[0], cubeVal0, cubesPos[4], cubeVal4);\n"
		"	vertlist[9] = vert_lerp(iso_level, cubesPos[1], cubeVal1, cubesPos[5], cubeVal5);\n");
	str_cat(&shader_buffer,
		"	vertlist[10] = vert_lerp(iso_level, cubesPos[2], cubeVal2, cubesPos[6], cubeVal6);\n"
		"	vertlist[11] = vert_lerp(iso_level, cubesPos[3], cubeVal3, cubesPos[7], cubeVal7);\n");

	str_cat(&shader_buffer,
		"	for(int i = 0; i < 5; i++)\n"
		"	{\n"
		"		ivec3 I = triTableValue(cubeindex, i);\n"
		"		mat4 MV = camera(view) * model;\n"
		"		if(I.x != -1)\n"
		"		{\n"
		"			vec4 pos0;\n"
		"			vec4 pos1;\n"
		"			vec4 pos2;\n"
		"			vec3 wpos0;\n"
		"			vec3 wpos1;\n"
		"			vec3 wpos2;\n"
		"			vec3 cpos0;\n"
		"			vec3 cpos1;\n"
		"			vec3 cpos2;\n"
		"			vec3 norm0;\n"
		"			vec3 norm1;\n"
		"			vec3 norm2;\n");

	str_cat(&shader_buffer,
		"			pos0 = vec4(vertlist[I.x], 1.0);\n"
		"			norm0 = isonormal(pos0.xyz);\n"
		"			norm0 = (MV * vec4(norm0, 0.0f)).xyz;\n"
		"			pos0 = model * pos0;\n"
		"			wpos0 = pos.xyz;\n"
		"			pos0 = camera(view) * pos0;\n"
		"			cpos0 = pos0.xyz;\n"
		"			pos0 = camera(projection) * pos0;\n");

	str_cat(&shader_buffer,
		"			pos1 = vec4(vertlist[I.y], 1.0);\n"
		"			norm1 = isonormal(pos1.xyz);\n"
		"			norm1 = (MV * vec4(norm1, 0.0f)).xyz;\n"
		"			pos1 = model * pos1;\n"
		"			wpos1 = pos1.xyz;\n"
		"			pos1 = camera(view) * pos1;\n"
		"			cpos1 = pos1.xyz;\n"
		"			pos1 = camera(projection) * pos1;\n");

	str_cat(&shader_buffer,
		"			pos2 = vec4(vertlist[I.z], 1.0);\n"
		"			norm2 = isonormal(pos2.xyz);\n"
		"			norm2 = (MV * vec4(norm2, 0.0f)).xyz;\n"
		"			pos2 = model * pos2;\n"
		"			wpos2 = pos2.xyz;\n"
		"			pos2 = camera(view) * pos2;\n"
		"			cpos2 = pos2.xyz;\n"
		"			pos2 = camera(projection) * pos2;\n");

	/* vertex_normal = normalize(cross(cpos1.xyz - cpos0.xyz, cpos2.xyz - cpos0.xyz)); */

	str_cat(&shader_buffer,
		"			vertex_normal = norm0;\n"
		"			poly_color = get_cubeColor(vertlist[I.x]);\n"
		"			vertex_world_position = wpos0;\n"
		"			vertex_position = cpos0;\n"
		"			gl_Position = pos0;\n"
		"			EmitVertex();\n");

	str_cat(&shader_buffer,
		"			vertex_world_position = wpos1;\n"
		"			vertex_position = cpos1;\n"
		"			poly_color = get_cubeColor(vertlist[I.y]);\n"
		"			vertex_normal = norm1;\n"
		"			gl_Position = pos1;\n"
		"			EmitVertex();\n");

	str_cat(&shader_buffer,
		"			vertex_world_position = wpos2;\n"
		"			vertex_position = cpos2;\n"
		"			poly_color = get_cubeColor(vertlist[I.z]);\n"
		"			vertex_normal = norm2;\n"
		"			gl_Position = pos2;\n"
		"			EmitVertex();\n");

	str_cat(&shader_buffer,
		"			EndPrimitive();\n"
		"		}\n"
		"		else\n"
		"		{\n"
		"			break;\n"
		"		}\n"
		"	}\n"
		"}\n");

	shader_add_source("candle:marching.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

static
void shaders_candle_pbr()
{
	char *shader_buffer = str_new(64);

	str_cat(&shader_buffer,
		"#include \"candle:common.glsl\"\n"
		"layout (location = 0) out vec4 FragColor;\n"
		"BUFFER {\n"
		"	sampler2D depth;\n"
		"	sampler2D albedo;\n"
		"	sampler2D nn;\n"
		"	sampler2D mr;\n"
		"} gbuffer;\n");

	str_cat(&shader_buffer,
		"void main(void)\n"
		"{\n"
		"	ivec2 fc = ivec2(gl_FragCoord.xy);\n"
		"	vec4 dif = texelFetch(gbuffer.albedo, fc, 0);\n"
		"	if(dif.a == 0.0) discard;\n");

	str_cat(&shader_buffer,
		"	vec3 c_pos = get_position(gbuffer.depth);\n"
		"	vec3 w_pos = (camera(model) * vec4(c_pos, 1.0)).xyz;\n"
		"	vec2 normal = texelFetch(gbuffer.nn, fc, 0).rg;\n"
		"	vec2 metalic_roughness = texelFetch(gbuffer.mr, fc, 0).rg;\n"
		"	vec3 c_nor = decode_normal(normal);\n");

	str_cat(&shader_buffer,
		"	float dist_to_eye = length(c_pos);\n"
		"	vec3 color = vec3(0.0);\n"
		"	if(light(radius) < 0.0)\n"
		"	{\n"
		"		color = light(color.rgb) * dif.xyz;\n"
		"	}\n"
		"	else if(light(color).r > 0.01 || light(color).g > 0.01 || light(color).b > 0.01)\n"
		"	{\n"
		"		vec3 c_light = (camera(view) * vec4(obj_pos, 1.0)).xyz;\n"
		"		vec3 c_light_dir = c_light - c_pos;\n"
		"		vec3 w_light_dir = obj_pos - w_pos;\n"
		"		float point_to_light = length(c_light_dir);\n");
	str_cat(&shader_buffer,
		"		float depth = textureLod(gbuffer.depth, pixel_pos(), 0.0).r;\n"
		"		float sd = 0.0;\n"
		"		if(dif.a >= 1.0)\n"
		"		{\n"
		"			sd = get_shadow(w_light_dir, point_to_light, dist_to_eye, depth);\n"
		"		}\n");
	str_cat(&shader_buffer,
		"		if(sd < 0.95)\n"
		"		{\n"
		"			vec3 eye_dir = normalize(-c_pos);\n"
		"			float l = point_to_light / light(radius);\n"
		"			float attenuation = ((3.0 - l*3.0) / (1.0+1.3*(pow(l*3.0-0.1, 2.0))))/3.0;\n"
		"			attenuation = clamp(attenuation, 0.0, 1.0);\n"
		"			vec4 color_lit = pbr(dif, metalic_roughness,\n"
		"					light(color) * attenuation, c_light_dir, c_pos, c_nor);\n"
		"			color += color_lit.xyz * (1.0 - sd);\n"
		"		}\n"
		"	}\n"
		"\n"
		"	FragColor = vec4(color, 1.0);\n"
		"}\n");

	shader_add_source("candle:pbr.glsl", shader_buffer,
	                  str_len(shader_buffer));
	str_free(shader_buffer);
}

void shaders_candle()
{
	shaders_candle_uniforms();
	shaders_candle_final();
	shaders_candle_ssao();
	shaders_candle_copy();
	shaders_candle_downsample();
	shaders_candle_upsample();
	shaders_candle_kawase();
	shaders_candle_bright();
	shaders_candle_framebuffer_draw();
	shaders_candle_quad();
	shaders_candle_motion();
	shaders_candle_uniforms();
	shaders_candle_common();
	shaders_candle_volum();
	shaders_candle_gaussian();
	shaders_candle_marching();
	shaders_candle_pbr();
}
