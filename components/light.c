#include "light.h"
#include "node.h"
#include "spatial.h"
#include "model.h"
#include <utils/drawable.h>
#include <candle.h>
#include <systems/editmode.h>
#include <systems/window.h>
#include <stdlib.h>
#include <systems/render_device.h>
#include <components/sprite.h>
#include <utils/bulb.h>

static mesh_t *g_light;
extern vs_t *g_quad_vs;
extern mesh_t *g_quad_mesh;
/* entity_t g_light = entity_null; */
static int g_lights_num;
static mat_t *g_light_widget;

static int c_light_position_changed(c_light_t *self);
static int c_light_editmode_toggle(c_light_t *self);

c_light_t *c_light_new(float radius, vec4_t color, uint32_t shadow_size)
{
	c_light_t *self = component_new("light");

	self->color = color;
	self->shadow_size = shadow_size;
	self->radius = radius;
	self->volumetric_intensity = 0.0f;

	return self;
}

void c_light_init(c_light_t *self)
{
	self->color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	self->shadow_size = 512;
	self->radius = 5.0f;
	self->volumetric_intensity = 0.0f;
	self->visible = 1;
	self->shadow_cooldown = 1;

	self->ambient_group = ref("ambient");
	self->light_group = ref("light");
	self->visible_group = ref("shadow");

	if(!g_light)
	{
		g_light = mesh_new();
		mesh_lock(g_light);
		mesh_ico(g_light, -0.5f);
		mesh_select(g_light, SEL_EDITING, MESH_FACE, -1);
		mesh_subdivide(g_light, 1);
		mesh_spherize(g_light, 1.0f);
		mesh_unlock(g_light);

		g_light_widget = mat_new("light_widget", "default");
		mat1t(g_light_widget, ref("albedo.texture"),
		      texture_from_memory(bulb_png, bulb_png_len));
		mat1f(g_light_widget, ref("albedo.blend"), 1.0f);
		mat4f(g_light_widget, ref("emissive.color"), vec4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	self->id = g_lights_num++;

	drawable_init(&self->widget, ref("visible"));
	drawable_add_group(&self->widget, ref("selectable"));
	drawable_set_vs(&self->widget, sprite_vs());
	drawable_set_mat(&self->widget, g_light_widget);
	drawable_set_entity(&self->widget, c_entity(self));
	drawable_set_xray(&self->widget, true);

	drawable_init(&self->draw, self->light_group);
	drawable_set_vs(&self->draw, model_vs());
	drawable_set_mesh(&self->draw, g_light);
	drawable_set_matid(&self->draw, self->id);
	drawable_set_entity(&self->draw, c_entity(self));

	c_light_position_changed(self);
	c_light_editmode_toggle(self);
	world_changed();

}

void c_light_set_lod(c_light_t *self, uint32_t lod)
{
	if (self->lod != lod)
	{
		self->lod = lod;
		self->frames_passed = self->shadow_cooldown;
	}
}

probe_tile_t get_free_tile(uint32_t level, uint32_t *result_level);
void release_tile(probe_tile_t tile);
uint32_t get_level_size(uint32_t level);

extern texture_t *g_probe_cache;
static void c_light_create_renderer(c_light_t *self)
{
	renderer_t *renderer = renderer_new(1.0f);

	self->tile = get_free_tile(0, &self->lod);

	texture_t *output =	g_probe_cache;

	renderer_add_pass(renderer, "depth", "depth", self->visible_group,
			CULL_DISABLE, output, output, 0, ~0,
			(bind_t[]){
				{CLEAR_DEPTH, .number = 1.0f},
				{CLEAR_COLOR, .vec4 = vec4(0.343750, 0.996094, 0.996094, 0.000000)},
				{NONE}
			}
	);
	renderer->pos = self->tile.pos;
	renderer->size = self->tile.size;
	renderer->cubemap = true;

	renderer_resize(renderer, self->shadow_size, self->shadow_size);
	renderer->output = output;

	/* renderer_set_output(renderer, output); */

	self->renderer = renderer;
}

void c_light_set_shadow_cooldown(c_light_t *self, uint32_t cooldown)
{
	self->shadow_cooldown = cooldown;
}

void c_light_set_groups(c_light_t *self, uint32_t visible_group,
		uint32_t ambient_group, uint32_t light_group)
{
	self->visible_group = visible_group;
	if(!self->renderer)
	{
		c_light_create_renderer(self);
	}
	self->renderer->passes[0].draw_signal = visible_group;

	self->ambient_group = ambient_group;
	self->light_group = light_group;
	self->modified = 1;

}

void c_light_visible(c_light_t *self, uint32_t visible)
{
	drawable_set_mesh(&self->draw, visible ? g_light : NULL);
	if(!self->visible && visible)
	{
		self->frames_passed = self->shadow_cooldown;
		drawable_model_changed(&self->draw);
	}
	self->visible = visible;
}

static int c_light_position_changed(c_light_t *self)
{
	c_node_t *node = c_node(self);
	c_node_update_model(node);
	drawable_set_transform(&self->widget, node->model);
	self->modified = 1;
	return CONTINUE;
}

static int c_light_pre_draw(c_light_t *self)
{
	/* if(!self->modified) return CONTINUE; */
	if(self->radius == -1)
	{
		drawable_set_group(&self->draw, self->ambient_group);
		drawable_set_vs(&self->draw, g_quad_vs);
		if(self->visible)
		{
			drawable_set_mesh(&self->draw, g_quad_mesh);
		}
	}
	else
	{
		self->frames_passed++;
		if(self->frames_passed >= self->shadow_cooldown)
		{
			self->frames_passed = 0;
		}
		else
		{
			return CONTINUE;
		}

		if(!self->renderer)
		{
			c_light_create_renderer(self);
		}
		drawable_set_group(&self->draw, self->light_group);
		drawable_set_vs(&self->draw, model_vs());
		if(self->visible)
		{
			drawable_set_mesh(&self->draw, g_light);
		}
		c_node_t *node = c_node(self);
		vec3_t pos = c_node_pos_to_global(node, vec3(0, 0, 0));
		mat4_t model = mat4_translate(pos);
		float scale = self->radius * 1.15f;
		model = mat4_scale_aniso(model, vec3(scale, scale, scale));

		drawable_set_transform(&self->draw, model);

		renderer_set_model(self->renderer, 0, &node->model);
		/* renderer_pass(self->renderer, ref("depth"))->clear_color = */
			/* vec4(0, 0, 0, self->radius); */
		renderer_draw(self->renderer);
	}
	self->modified = 0;
	return CONTINUE;
}

int c_light_menu(c_light_t *self, void *ctx)
{
	nk_layout_row_dynamic(ctx, 0, 1);

	int ambient = self->radius == -1.0f;
	nk_checkbox_label(ctx, "ambient", &ambient);

	if(!ambient)
	{
		float rad = self->radius;
		if (rad < 0.0f) rad = 0.0f;
		float volum = self->volumetric_intensity;
		if(self->radius < 0.0f) self->radius = 0.01;
		nk_property_float(ctx, "radius:", 0.01, &rad, 1000, 0.1, 0.05);
		if(rad != self->radius)
		{
			self->radius = rad;
			c_light_position_changed(self);
			world_changed();
		}
		nk_property_float(ctx, "volumetric:", -1.0, &volum, 10.0, 0.1, 0.05);
		if(volum != self->volumetric_intensity)
		{
			self->volumetric_intensity = volum;
			world_changed();
		}

	}
	else
	{
		self->radius = -1;
	}

	nk_layout_row_dynamic(ctx, 180, 1);

	struct nk_colorf *old = (struct nk_colorf *)&self->color;
	union { struct nk_colorf nk; vec4_t v; } new =
		{ .nk = nk_color_picker(ctx, *old, NK_RGBA)};

	if(memcmp(&new, &self->color, sizeof(vec4_t)))
	{
		self->color = new.v;
		world_changed();
	}

	return CONTINUE;
}

static void c_light_destroy(c_light_t *self)
{
	release_tile(self->tile);
	drawable_set_mesh(&self->draw, NULL);
	drawable_set_mesh(&self->widget, NULL);
	if(self->renderer)
	{
		renderer_destroy(self->renderer);
	}
}

static int c_light_editmode_toggle(c_light_t *self)
{
	c_editmode_t *edit = c_editmode(&SYS);
	if(!edit) return CONTINUE;

	if(edit->control)
	{
		drawable_set_mesh(&self->widget, sprite_mesh());
	}
	else
	{
		drawable_set_mesh(&self->widget, NULL);
	}
	return CONTINUE;
}

REG()
{
	ct_t *ct = ct_new("light", sizeof(c_light_t), c_light_init,
			c_light_destroy, 1, ref("node"));

	ct_listener(ct, WORLD, 0, sig("editmode_toggle"), c_light_editmode_toggle);
	ct_listener(ct, WORLD, 0, sig("component_menu"), c_light_menu);
	ct_listener(ct, WORLD, 0, sig("world_pre_draw"), c_light_pre_draw);
	ct_listener(ct, ENTITY, 0, sig("node_changed"), c_light_position_changed);
}
