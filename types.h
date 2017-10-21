/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#ifndef __TYPES_H__
#define __TYPES_H__
#include <stdint.h>

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

struct fbo {
	unsigned int width, height;
	GLuint handle;
	GLuint texture;
};

struct viewport {
	unsigned int x, y, w, h;
};

struct attr {
	GLuint loc;
	GLint size;
	GLsizei stride;
	GLvoid *ptr;
};

#endif /* __TYPES_H__ */
