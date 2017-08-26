/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#ifndef __TEXTURE_H__
#define __TEXTURE_H__
#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

struct texture {
	uint32_t width, height;
	size_t datalen;

	char *data;
	GLuint handle;
};

struct texture *texture_load(const char *file);

#endif /* __TEXTURE_H__ */
