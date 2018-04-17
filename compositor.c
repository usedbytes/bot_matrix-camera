/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <string.h>
#include "compositor.h"
#include "shader.h"
#include "texture.h"
#include "list.h"
#include "drawcall.h"
#include "EGL/egl.h"

struct compositor {
	struct list_head layers;
	struct fbo *fbo;
	struct viewport vp;

	GLint shader, posLoc, tcLoc, mvpLoc, texLoc;
};

struct layer {
	struct list_head list;

	struct compositor *cmp;
	struct drawcall *dc;
	GLfloat mvp[16];
};

void compositor_set_viewport(struct compositor *cmp, struct viewport *vp) {
	cmp->vp = *vp;
}

struct compositor *compositor_create(struct fbo *fbo)
{
	struct compositor *cmp = calloc(1, sizeof(*cmp));
	if (!cmp) {
		return NULL;
	}

	list_init(&cmp->layers);
	cmp->fbo = fbo;
	cmp->shader = shader_load_compile_link("compositor_vs.glsl", "compositor_fs.glsl");
	if (cmp->shader <= 0) {
		fprintf(stderr, "Couldn't get compositor shaders\n");
		free(cmp);
		return NULL;
	}
	cmp->vp = (struct viewport){0, 0, fbo->width, fbo->height};

	glUseProgram(cmp->shader);
	cmp->posLoc = glGetAttribLocation(cmp->shader, "position");
	cmp->tcLoc = glGetAttribLocation(cmp->shader, "tc");
	cmp->mvpLoc = glGetUniformLocation(cmp->shader, "mvp");
	cmp->texLoc = glGetUniformLocation(cmp->shader, "tex");
	glUniform1i(cmp->texLoc, 0);
	glUseProgram(0);

	return cmp;
}

void compositor_draw(struct compositor *cmp)
{
	struct list_head *node = cmp->layers.next;

	glBindFramebuffer(GL_FRAMEBUFFER, cmp->fbo->handle);
	glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	while (node != &cmp->layers) {
		struct layer *layer = (struct layer *)node;

		/*
		 * All the layers share a shader, so we need to set the mvp
		 * here
		 */
		drawcall_set_mvp(layer->dc, layer->mvp);
		drawcall_draw(layer->dc);

		node = node->next;
	}
}

static struct drawcall *get_drawcall(struct compositor *cmp)
{
	struct drawcall *dc = calloc(1, sizeof(*dc));
	GLuint vertices, indices;
	const GLfloat quad[] = {
		-1.0f,  -1.0f,  0.0f,  0.0f,
		 1.0f,  -1.0f,  1.0f,  0.0f,
		-1.0f,   1.0f,  0.0f,  1.0f,
		 1.0f,   1.0f,  1.0f,  1.0f,
	};
	const GLshort idx[] = {
		0, 1, 2, 3,
	};

	if (!dc) {
		return NULL;
	}

	dc->n_attributes = 2;
	dc->attributes[0] = (struct attr){
		.loc = cmp->posLoc,
		.size = 2,
		.stride = sizeof(quad[0]) * 4,
		.ptr = (GLvoid *)0,
	};
	dc->attributes[1] = (struct attr){
		.loc = cmp->tcLoc,
		.size = 2,
		.stride = sizeof(quad[0]) * 4,
		.ptr = (GLvoid *)(sizeof(quad[0]) * 2),
	};

	dc->n_buffers = 2;
	glGenBuffers(1, &vertices);
	dc->buffers[0] = (struct bind){ .bind = GL_ARRAY_BUFFER, .handle = vertices };
	glGenBuffers(1, &indices);
	dc->buffers[1] = (struct bind){ .bind = GL_ELEMENT_ARRAY_BUFFER, .handle = indices };
	dc->n_indices = sizeof(idx) / sizeof(idx[0]);

	dc->draw = draw_elements;
	dc->fbo = cmp->fbo;

	dc->shader_program = cmp->shader;
	dc->mvp = glGetUniformLocation(dc->shader_program, "mvp");

	glUseProgram(dc->shader_program);

	glBindBuffer(GL_ARRAY_BUFFER, vertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUniform1i(cmp->texLoc, 0);

	glUseProgram(0);

	return dc;
}

/* Identity by default. */
const GLfloat eye[] = {
	1.0f,  0.0f,  0.0f,  0.0f,
	0.0f,  1.0f,  0.0f,  0.0f,
	0.0f,  0.0f,  1.0f,  0.0f,
	0.0f,  0.0f,  0.0f,  1.0f,
};

struct layer *compositor_create_layer(struct compositor *cmp)
{
	struct layer *layer = calloc(1, sizeof(*layer));
	if (!layer) {
		return NULL;
	}

	layer->cmp = cmp;
	layer->dc = get_drawcall(cmp);
	if (!layer->dc) {
		free(layer);
		return NULL;
	}

	layer_set_transform(layer, eye);
	layer_set_display_rect(layer, 0.0, 0.0, 1.0, 1.0);
	layer_set_texture(layer, 0);

	list_add_end(&cmp->layers, (struct list_head *)layer);

	return layer;
}

void layer_set_texture(struct layer *layer, GLint handle)
{
	layer->dc->n_textures = 1;
	layer->dc->textures[0] = (struct bind){ .bind = GL_TEXTURE_2D, .handle = handle };
}

void layer_set_display_rect(struct layer *layer, float x, float y, float w, float h)
{
	layer->dc->viewport.x = layer->cmp->vp.x + layer->cmp->vp.w * x;
	layer->dc->viewport.y = layer->cmp->vp.y + layer->cmp->vp.h * y;
	layer->dc->viewport.w = layer->cmp->vp.w * w;
	layer->dc->viewport.h = layer->cmp->vp.h * h;
}

void layer_set_transform(struct layer *layer, const GLfloat *m4)
{
	memcpy(layer->mvp, m4, sizeof(layer->mvp));
}
