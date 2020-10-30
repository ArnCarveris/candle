#include <utils/glutil.h>
#include "window.h"
#include <utils/shader.h>
#include <utils/drawable.h>
#include <candle.h>
#include <components/model.h>
#include <components/node.h>
#include <systems/render_device.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <GLFW/glfw3.h>

#include <utils/gl.h>

int window_width = 1360;
int window_height = 766;
//
// This is free and unencumbered software released into the public domain.
// 
// Anyone is free to copy, modify, publish, use, compile, sell, or distribute
// this software, either in source code form or as a compiled binary, for any
// purpose, commercial or non-commercial, and by any means.
// 
// In jurisdictions that recognize copyright laws, the author or authors of this
// software dedicate any and all copyright interest in the software to the
// public domain. We make this dedication for the benefit of the public at large
// and to the detriment of our heirs and successors. We intend this dedication
// to be an overt act of relinquishment in perpetuity of all present and future
// rights to this software under copyright law.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// For more information, please refer to <http://unlicense.org/>

// REQUIREMENTS: OpenGL version with the KHR_debug extension available.

// Callback function for printing debug statements
static void GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length,
		const GLchar *msg, const void *data)
{
	puts(msg);
}
vs_t *g_quad_vs;
fs_t *g_quad_fs;
mesh_t *g_quad_mesh;

static void init_context_b(c_window_t *self)
{
	const GLubyte *renderer;
	const GLubyte *glvendor;
	const GLubyte *version;
	const GLubyte *glslVersion;
/* #ifdef __EMSCRIPTEN__ */

/* 	emscripten_set_canvas_element_size("#canvas", self->width, self->height); */

/* 	EmscriptenWebGLContextAttributes attrs; */
/* 	emscripten_webgl_init_context_attributes(&attrs); */
/* 	attrs.antialias = false; */
/* 	attrs.majorVersion = 2; */
/* 	attrs.minorVersion = 0; */
/* 	attrs.alpha = false; */
/* 	attrs.depth = true; */
/* 	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attrs); */

/* 	if (emscripten_webgl_enable_extension(ctx, "EXT_color_buffer_float") == EM_FALSE) printf("%d\n", __LINE__); */
/* 	if (emscripten_webgl_enable_extension(ctx, "GL_EXT_color_buffer_float") == EM_FALSE) printf("%d\n", __LINE__); */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "EXT_texture_filter_anisotropic") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_EXT_texture_filter_anisotropic") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "OES_texture_float_linear") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_OES_texture_float_linear") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "WEBGL_lose_context") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "WEBGL_depth_texture") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	/1* if (emscripten_webgl_enable_extension(ctx, "GL_WEBGL_lose_context") == EM_FALSE) printf("%d\n", __LINE__); *1/ */
/* 	emscripten_webgl_make_context_current(ctx); */
/* #else */

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	/*glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);*/
	/*glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);*/
	/*glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);*/

	self->window = glfwCreateWindow(self->width, self->height, "Candle", NULL, NULL);
	glfwMakeContextCurrent(self->window);
	glfwGetWindowSize(self->window, &self->width, &self->height);
	if (!self->window)
	{
		printf("Window failed to be created\n");
		exit(1);
	}

	glInit();

	glDepthFunc(GL_LESS); glerr();
	renderer = glGetString( GL_RENDERER );
	glvendor = glGetString( GL_VENDOR );
	version = glGetString( GL_VERSION );
	glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

	printf("GL Vendor : %s\n", glvendor);
	printf("GL Renderer : %s\n", renderer);
	printf("GL Version (string) : %s\n", version);
	printf("GLSL Version : %s\n", glslVersion); 

	if (false)
	{
		glEnable(0x92E0);
		glEnable(0x8242);
		glDebugMessageCallback(GLDebugMessageCallback, NULL);
	}
}

void c_window_init(c_window_t *self)
{
	self->width = window_width;
	self->height = window_height;

}

int c_window_toggle_fullscreen_gl(c_window_t *self)
{
	/* sdl_DisplayMode dm; */
	/* window_resize_data wdata; */

	/* self->fullscreen = !self->fullscreen; */

	/* if(sdl_GetDesktopDisplayMode(0, &dm) != 0) */
	/* { */
	/* 	sdl_Log("sdl_GetDesktopDisplayMode failed: %s", sdl_GetError()); */
	/* 	return 1; */
	/* } */
	/* self->width = dm.w; */
	/* self->height = dm.h; */

	/* wdata.width = self->width; */
	/* wdata.height = self->height; */
	/* entity_signal(entity_null, sig("window_resize"), &wdata, NULL); */

	/* sdl_SetWindowSize(self->window, self->width, self->height); */

	/* sdl_SetWindowFullscreen(self->window, */
	/* 	self->fullscreen?sdl_WINDOW_FULLSCREEN_DESKTOP:0); */

	return 1;
}

void c_window_toggle_fullscreen(c_window_t *self)
{
	loader_push(g_candle->loader, (loader_cb)c_window_toggle_fullscreen_gl,
		NULL, (c_t*)self);
}

int c_window_created(c_window_t *self)
{
	return CONTINUE;
}

c_window_t *c_window_new(int width, int height)
{
	c_window_t *self = component_new(ct_window);
	self->width = width ? width : window_width;
	self->height = height ? height : window_height;

	init_context_b(self);
	if(!g_quad_vs)
	{
		g_quad_vs = vs_new("candle:quad", false, 1, vertex_modifier_new(
			/* "texcoord *= screen_size;\n" */
			""
		));
		g_quad_fs = fs_new("candle:quad");
	}

	g_quad_mesh = mesh_new();
	mesh_quad(g_quad_mesh);
	g_quad_mesh->cull = 0;

	drawable_init(&self->draw, ref("quad"));
	drawable_set_entity(&self->draw, c_entity(self));
	drawable_set_mesh(&self->draw, g_quad_mesh);
	drawable_set_vs(&self->draw, g_quad_vs);

	return self;
}

void c_window_lock_fps(c_window_t *self, int32_t lock_fps)
{
	glfwSwapInterval(lock_fps);
}

int c_window_draw_end(c_window_t *self)
{
	glfwSwapBuffers(self->window);

	return CONTINUE;
}

int c_window_draw(c_window_t *self)
{
	texture_t *tex;
	shader_t *shader;
	uint32_t uni, ss;

	if(!self->renderer) return CONTINUE;

	/* renderer_draw(self->renderer); */

	tex = self->renderer->output;

	if(!tex)
	{
		printf("No texture to draw\n");
		return CONTINUE;
	}

	/* glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE); glerr(); */
	glBindFramebuffer(GL_FRAMEBUFFER, 0); glerr();
	glClear(GL_DEPTH_BUFFER_BIT); glerr();

	fs_bind(g_quad_fs);

	glViewport(0, 0, self->width, self->height); glerr();

	shader = vs_bind(g_quad_vs, 0);
	if (!shader)
		return CONTINUE;

	uni = shader_cached_uniform(shader, ref("tex"));
	glUniform1i(uni, 0);
	glActiveTexture(GL_TEXTURE0);
	texture_bind(tex, tex->draw_id);

	ss = shader_cached_uniform(shader, ref("screen_size"));
	glUniform2f(ss, self->width, self->height); glerr();

	drawable_draw(&self->draw);
	glerr();

	glActiveTexture(GL_TEXTURE0); glerr();
	glBindTexture(GL_TEXTURE_2D, 0); glerr();

	return CONTINUE;
}

int32_t c_window_event(c_window_t *self, const candle_event_t *event)
{
	if (event->type != CANDLE_WINDOW_RESIZE)
	{
		return CONTINUE;
	}
	{
		window_resize_data wdata;

		self->width = event->window.width;
		self->height = event->window.height;
		if(self->renderer)
		{
			renderer_resize(self->renderer, self->width, self->height);
		}
		wdata.width = self->width;
		wdata.height = self->height;
		entity_signal(entity_null, sig("window_resize"), &wdata, NULL);
		return STOP;
	}
	return CONTINUE;
}

void ct_window(ct_t *self)
{
	ct_init(self, "window", sizeof(c_window_t));
	ct_set_init(self, (init_cb)c_window_init);

	ct_add_listener(self, ENTITY, 0, ref("entity_created"), c_window_created);
	ct_add_listener(self, WORLD, 5, ref("world_draw"), c_window_draw);
	ct_add_listener(self, WORLD, 5, ref("world_draw_end"), c_window_draw_end);

	ct_add_listener(self, WORLD, 250, ref("event_handle"), c_window_event);
}
