/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

	/*
	dc->textures[dc->yidx] = feed->ytex;
	dc->textures[dc->uidx] = feed->utex;
	dc->textures[dc->vidx] = feed->vtex;
	*/

	if (dc->fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, dc->fbo->handle);
	}
	glViewport(dc->viewport.x, dc->viewport.y, dc->viewport.w, dc->viewport.h);

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

	dc->textures[dc->yidx] = (struct bind){ "", 0, 0 };
	dc->textures[dc->uidx] = (struct bind){ "", 0, 0 };
	dc->textures[dc->vidx] = (struct bind){ "", 0, 0 };

	if (dc->fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glUseProgram(0);
}

struct drawcall *drawcall_create(GLint shader)
{
	GLuint handles[2];
	struct drawcall *dc = calloc(1, sizeof(*dc));
	if (!dc) {
		return NULL;
	}

	dc->yidx = dc->uidx = dc->vidx = -1;

	/* Set up handles for vertices and indices. Data will get added later */
	glGenBuffers(2, handles);
	dc->n_buffers = 2;
	dc->buffers[0] = (struct bind){ .bind = GL_ARRAY_BUFFER, .handle = handles[0] };
	dc->buffers[1] = (struct bind){ .bind = GL_ELEMENT_ARRAY_BUFFER, .handle = handles[1] };

	dc->shader_program = shader;

	dc->mvp = glGetUniformLocation(dc->shader_program, "mvp");

	dc->draw = draw_elements;

	return dc;
}

void drawcall_set_attribute(struct drawcall *dc, const char *name,
		GLint size, GLsizei stride, GLvoid *ptr)
{
	struct attr *attr = NULL;
	unsigned int i;

	glUseProgram(dc->shader_program);

	for (i = 0; i < dc->n_attributes; i++) {
		if (!strcmp(name, dc->attributes[i].name)) {
			attr = &dc->attributes[i];
			break;
		}
	}
	if (!attr) {
		attr = &dc->attributes[dc->n_attributes];
		strncpy(attr->name, name, sizeof(attr->name));
		attr->loc = glGetAttribLocation(dc->shader_program, attr->name);

		dc->n_attributes++;
	}

	attr->size = size;
	attr->stride = stride;
	attr->ptr = ptr;

	glUseProgram(0);
}

static void drawcall_set_buffer_data(struct drawcall *dc, GLuint target, const GLvoid *data, GLsizei size)
{
	struct bind *bind = NULL;
	unsigned int i;

	for (i = 0; i < dc->n_buffers; i++) {
		if (dc->buffers[i].bind == target) {
			bind = &dc->buffers[i];
			break;
		}
	}
	if (!bind) {
		bind = &dc->buffers[dc->n_buffers];
		bind->bind = target;
		glGenBuffers(1, &bind->handle);

		dc->n_buffers++;
	}

	glBindBuffer(bind->bind, bind->handle);
	// TODO: Do we want to allow usage to be set?
	glBufferData(bind->bind, size, data, GL_STATIC_DRAW);
	glBindBuffer(bind->bind, 0);
}

void drawcall_set_vertex_data(struct drawcall *dc, const GLvoid *data, GLsizei size)
{
	drawcall_set_buffer_data(dc, GL_ARRAY_BUFFER, data, size);
}

void drawcall_set_indices(struct drawcall *dc, const GLvoid *data, GLsizei size, unsigned int nidx)
{
	drawcall_set_buffer_data(dc, GL_ELEMENT_ARRAY_BUFFER, data, size);
	dc->n_indices = nidx;
}

void drawcall_set_texture(struct drawcall *dc, const char *name,
		GLuint target, GLint handle)
{
	struct bind *bind = NULL;
	unsigned int i;

	glUseProgram(dc->shader_program);

	for (i = 0; i < dc->n_textures; i++) {
		if (!strcmp(dc->textures[i].name, name)) {
			bind = &dc->textures[i];
			break;
		}
	}
	if (!bind) {
		GLint loc;

		bind = &dc->textures[dc->n_textures];
		strncpy(bind->name, name, 32);
		loc = glGetUniformLocation(dc->shader_program, name);
		glUniform1i(loc, dc->n_textures);

		dc->n_textures++;
	}

	bind->bind = target;
	bind->handle = handle;

	glUseProgram(0);
}

void drawcall_set_fbo(struct drawcall *dc, struct fbo *fbo)
{
	dc->fbo = fbo;
}

void drawcall_set_viewport(struct drawcall *dc, GLuint x, GLuint y, GLuint w, GLuint h)
{
	dc->viewport.x = x;
	dc->viewport.y = y;
	dc->viewport.w = w;
	dc->viewport.h = h;
}

void drawcall_set_mvp(struct drawcall *dc, const GLfloat *mvp)
{
	glUseProgram(dc->shader_program);
	glUniformMatrix4fv(dc->mvp, 1, GL_FALSE, mvp);
	glUseProgram(0);
}

void drawcall_destroy(struct drawcall *dc)
{
	while (dc->n_buffers--) {
		glDeleteBuffers(1, &dc->buffers[dc->n_buffers].handle);
	}
}
