
layout (location = 0) out vec4 DiffuseColor;
layout (location = 1) out vec4 SpecularColor;
layout (location = 2) out vec3 NormalColor;
layout (location = 3) out vec4 Transparency;
layout (location = 4) out vec3 CPositionColor;
layout (location = 5) out vec3 WPositionColor;
layout (location = 6) out vec4 ID;
layout (location = 7) out vec3 CNormal;

#include "common.frag"

void main()
{
	vec4 dif  = resolveProperty(diffuse, texcoord);
	if(dif.a == 0.0f) discard;

	float noi = (textureLod(perlin_map, (worldspace_position + 1) / 14, 0).r) * 4 - 3.5f;

	/* dif = vec4(dif.rgb * (1 - (noi / 10.0)), dif.a); */

	DiffuseColor = dif;
	SpecularColor = resolveProperty(specular, texcoord) * 2;
	/* SpecularColor.a *= 1.0 - clamp(abs(noi * n.y), 0.0f, 1.0f); */
	/* DiffuseColor = vec4(vec3(SpecularColor.a), 1.0f); */

	NormalColor = normalize(get_normal());
	CNormal = normalize(get_cnormal());

	WPositionColor = worldspace_position;
	CPositionColor = cameraspace_vertex_pos;

	/* ID = vec4(object_id.x, object_id.y, poly_id.x, poly_id.y); */
	ID = vec4(object_id.x, object_id.y, poly_id.x, poly_id.y);

	/* float up = max(n.y, 0.0); */
	/* DiffuseColor = vec4(vec3(up), 1.0); */
	Transparency = resolveProperty(transparency, texcoord);

	/* float mipmapLevel = textureQueryLod(diffuse.texture, texcoord).x; */
	/* Transparency = vec4(vec3(mipmapLevel/10), 1.0f); return; */

}

// vim: set ft=c:
