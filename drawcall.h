/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#ifndef __DRAWCALL_H__
#define __DRAWCALL_H__
#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

#include "types.h"
#include "feed.h"

struct drawcall {
	GLuint shader_program;
	unsigned int n_buffers, n_textures, n_uniforms;
	int yidx, uidx, vidx;
	struct bind buffers[10];
	/* Massive hack... any -1 special indexes land in scratch */
	struct bind scratch;
	struct bind textures[10];
	struct bind uniforms[10];
	unsigned int n_indices;

	struct fbo fbo;
	struct viewport viewport;

	void (*draw)(struct drawcall *);
};

void draw_elements(struct drawcall *dc);

void drawcall_draw(struct feed *feed, struct drawcall *dc);

#endif /* __DRAWCALL_H__ */
