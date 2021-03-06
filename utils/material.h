#ifndef MATERIAL_H
#define MATERIAL_H

#include "texture.h"
#include "mafs.h"
struct vifunc;
struct vicall;

typedef struct mat
{
	char name[256];

	uint32_t type;
	uint32_t global_id;
	uint32_t id;
	uint32_t update_id;
	struct vifunc *sandbox;
	struct vicall *call;
} mat_t;

extern char g_mats_path[256];

mat_t *mat_new(const char *name, const char *type_name);
mat_t *mat_from_memory(const char *bytes, size_t bytes_num,
                       const char *filename);
mat_t *mat_from_dir(const char *name, const char *dirname);
bool_t mat_is_transparent(mat_t *self);
void mat_destroy(mat_t *self);
void mat1i(mat_t *self, uint32_t ref, int32_t value);
void mat1f(mat_t *self, uint32_t ref, float value);
void mat2f(mat_t *self, uint32_t ref, vec2_t value);
void mat3f(mat_t *self, uint32_t ref, vec3_t value);
void mat4f(mat_t *self, uint32_t ref, vec4_t value);
void mat1t(mat_t *self, uint32_t ref, texture_t *value);
void mat1s(mat_t *self, uint32_t ref, const char *value);
int mat_menu(mat_t *self, void *ctx);

void materials_init_vil(void);
void materials_reg(void);
uint32_t material_get_ubo(uint32_t mat_type);

extern uint32_t g_mats_num;
extern mat_t *g_mats[255];

#endif /* !MATERIAL_H */
