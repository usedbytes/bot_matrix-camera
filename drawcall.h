/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#ifndef __DRAWCALL_H__
#define __DRAWCALL_H__
#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

struct bind {
	GLuint bind;
	GLuint handle;
};

enum uniform_type {
	UNIFORM_1i = 0,
	UNIFORM_MAT_F4,
};

struct uniform {
	enum uniform_type type;
	GLuint handle;
};

struct drawcall {
	GLuint shader_program;
	unsigned int n_buffers, n_textures, n_uniforms;
	struct bind buffers[10];
	struct bind textures[10];
	struct bind uniforms[10];
	unsigned int n_indices;

	void (*draw)(struct drawcall *);
};

void draw_elements(struct drawcall *dc);

void drawcall_draw(struct drawcall *dc);

#endif /* __DRAWCALL_H__ */
