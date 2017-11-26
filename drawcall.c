/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>
#include <GLES/gl.h>

#include "feed.h"
#include "drawcall.h"

void draw_elements(struct drawcall *dc)
{
	glDrawElements(GL_TRIANGLE_STRIP, dc->n_indices, GL_UNSIGNED_SHORT, 0);
}

void drawcall_draw(struct feed *feed, struct drawcall *dc)
{
	int i;
	glUseProgram(dc->shader_program);

	dc->textures[dc->yidx] = feed->ytex;
	dc->textures[dc->uidx] = feed->utex;
	dc->textures[dc->vidx] = feed->vtex;

	if (dc->fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, dc->fbo->handle);
		glViewport(0, 0, dc->fbo->width, dc->fbo->height);
	} else {
		glViewport(dc->viewport.x, dc->viewport.y, dc->viewport.w, dc->viewport.h);
	}

	for (i = 0; i < dc->n_textures; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(dc->textures[i].bind, dc->textures[i].handle);
	}
	for (i = 0; i < dc->n_buffers; i++) {
		glBindBuffer(dc->buffers[i].bind, dc->buffers[i].handle);
	}
	for (i = 0; i < dc->n_attributes; i++) {
		struct attr *attr = &dc->attributes[i];
		glVertexAttribPointer(attr->loc, attr->size, GL_FLOAT, GL_FALSE, attr->stride, attr->ptr);
		glEnableVertexAttribArray(attr->loc);
	}

	dc->draw(dc);

	for (i = 0; i < dc->n_attributes; i++) {
		struct attr *attr = &dc->attributes[i];
		glDisableVertexAttribArray(attr->loc);
	}
	for (i = 0; i < dc->n_buffers; i++) {
		glBindBuffer(dc->buffers[i].bind, 0);
	}
	for (i = 0; i < dc->n_textures; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(dc->textures[i].bind, 0);
	}

	dc->textures[dc->yidx] = (struct bind){ 0, 0 };
	dc->textures[dc->uidx] = (struct bind){ 0, 0 };
	dc->textures[dc->vidx] = (struct bind){ 0, 0 };

	if (dc->fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glUseProgram(0);
}
