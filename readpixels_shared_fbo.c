/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#include <stdio.h>
#include <stdlib.h>
#include "shared_fbo.h"

#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

struct readpixels_fbo {
	struct fbo base;
	char *map;
};

#define ALIGN_UP(_x, _align) ((_x + (_align - 1)) & ~(_align - 1))

static uint32_t next_pow2(uint32_t x)
{
	return 1 << (32 - __builtin_clz(x - 1));
}

static uint32_t max(uint32_t a, uint32_t b)
{
	return a > b ? a : b;
}

struct fbo *shared_fbo_create(struct pint *pint, uint32_t width, uint32_t height) {
	struct readpixels_fbo *fbo = malloc(sizeof(*fbo));
	if (!fbo) {
		return NULL;
	}

	fbo->map = malloc(width * height * 4);
	if (!fbo->map) {
		free(fbo);
		return NULL;
	}

	fbo->base.width = width;
	fbo->base.height = height;

	glGenFramebuffers(1, &fbo->base.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->base.handle);
	glGenTextures(1, &fbo->base.texture);
	glBindTexture(GL_TEXTURE_2D, fbo->base.texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->base.texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Framebuffer not complete\n");
		goto fail;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return &fbo->base;

fail:
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteTextures(1, &fbo->base.texture);
	glDeleteFramebuffers(1, &fbo->base.handle);

	free(fbo->map);
	free(fbo);
	return NULL;
}

char *shared_fbo_map(struct fbo *base, int32_t *stride)
{
	struct readpixels_fbo *fbo = (struct readpixels_fbo *)base;
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->base.handle);
	glReadPixels(0, 0, base->width, base->height, GL_RGBA, GL_UNSIGNED_BYTE, fbo->map);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (stride) {
		*stride = base->width * 4;
	}

	return fbo->map;
}

void shared_fbo_unmap(struct fbo *base)
{
	/* Nothing to do */
	return;
}
