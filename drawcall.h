/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#ifndef __DRAWCALL_H__
#define __DRAWCALL_H__
#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

#include "types.h"
#include "list.h"
#include "feed.h"

struct drawcall {
	struct list_head list;

	GLuint shader_program;
	GLuint mvp;
	unsigned int n_buffers, n_textures, n_uniforms, n_attributes;
	int yidx, uidx, vidx;
	struct bind buffers[10];

	/* Massive hack... any -1 special indexes land in scratch */
	struct bind scratch;
	struct bind textures[10];
	struct bind uniforms[10];
	struct attr attributes[10];
	unsigned int n_indices;

	struct fbo *fbo;
	struct viewport viewport;

	void (*draw)(struct drawcall *);
};

void draw_elements(struct drawcall *dc);

void drawcall_draw(struct feed *feed, struct drawcall *dc);

struct drawcall *drawcall_create(GLint shader);
void drawcall_set_attribute(struct drawcall *dc, const char *name,
		GLint size, GLsizei stride, GLvoid *ptr);
void drawcall_set_vertex_data(struct drawcall *dc, const GLvoid *data, GLsizei size);
void drawcall_set_indices(struct drawcall *dc, const GLvoid *data, GLsizei size, unsigned int nidx);
void drawcall_set_texture(struct drawcall *dc, const char *name,
		GLuint target, GLint handle);
void drawcall_set_fbo(struct drawcall *dc, struct fbo *fbo);
void drawcall_set_viewport(struct drawcall *dc, GLuint x, GLuint y, GLuint w, GLuint h);
void drawcall_set_mvp(struct drawcall *dc, const GLfloat *mvp);
void drawcall_destroy(struct drawcall *dc);

#endif /* __DRAWCALL_H__ */
