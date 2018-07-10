
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform vec2 over_id;
uniform vec2 sel_id;
uniform vec2 over_poly_id;
uniform vec2 context_id;
uniform int mode;

#define EDIT_VERT	0
#define EDIT_EDGE	1
#define EDIT_FACE	2
#define EDIT_OBJECT	3

const vec2 zero = vec2(0.003922, 0.000000);

float filtered(vec2 c, vec2 fil)
{
	fil = round(fil * 255); 
	c = round(c * 255); 

	if(c.x != fil.x || c.y != fil.y)
	{
		return 0;
	}
	return 1;
}

bool is_equal(vec2 a, vec2 b)
{
	a = round(a * 255); 
	b = round(b * 255); 

	return (b.x == a.x && b.y == a.y);
}

BUFFER {
	sampler2D id;
	sampler2D geomid;
} sbuffer;

void main()
{
	vec2 c;
	vec2 c2;

	c = textureLod(sbuffer.id, pixel_pos(), 0).rg;
	c2 = textureLod(sbuffer.geomid, pixel_pos(), 0).rg;

	float over = filtered(c, over_id);

	float overp = filtered(c2, over_poly_id);

	const vec3 over_poly_color = vec3(0.1f, 0.01f, 0.4f);
	const vec3 over_color = vec3(0.08);

	if(is_equal(c, zero))
	{
		FragColor = vec4(vec3(0.5), 1.0f);
		return;
	}

	vec3 final = vec3(1.0f);
	/* if(sel_id.x > 0.0f || sel_id.y > 0.0f) */
	/* { */
	/* 	final -= sel_color * (1.0f - selected); */
	/* } */
	if(is_equal(sel_id, c))
	{
		if(mode != EDIT_OBJECT)
		{
			final += (over_poly_color * overp);
		}
	}
	else
	{
		final += over_color * over;
	}

	FragColor = vec4(final, 1.0f);
	/* FragColor = vec4(c * 30, 0.0f, 1.0f); */
}

// vim: set ft=c:
