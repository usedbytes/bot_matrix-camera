/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

#include "drawcall.h"

void draw_elements(struct drawcall *dc)
{
	glDrawElements(GL_TRIANGLE_STRIP, dc->n_indices, GL_UNSIGNED_SHORT, 0);
}

void drawcall_draw(struct drawcall *dc)
{
	int i;
	glUseProgram(dc->shader_program);
	for (i = 0; i < dc->n_textures; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(dc->textures[i].bind, dc->textures[i].handle);
	}
	for (i = 0; i < dc->n_buffers; i++) {
		glBindBuffer(dc->buffers[i].bind, dc->buffers[i].handle);
	}

	dc->draw(dc);

	for (i = 0; i < dc->n_buffers; i++) {
		glBindBuffer(dc->buffers[i].bind, 0);
	}
	for (i = 0; i < dc->n_textures; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(dc->textures[i].bind, 0);
	}
	glUseProgram(0);
}
