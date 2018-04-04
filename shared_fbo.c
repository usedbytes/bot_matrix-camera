/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#include <stdio.h>
#include "shared_fbo.h"

#include "interface/vcsm/user-vcsm.h"
#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "EGL/eglext_brcm.h"

struct vcsm_fbo {
	struct fbo base;
	EGLImageKHR img;
	unsigned int vcsm_handle;
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
	static struct egl_image_brcm_vcsm_info vcsm_info = { 0 };
	struct vcsm_fbo *fbo = malloc(sizeof(*fbo));
	if (!fbo) {
		return NULL;
	}

	/* VCSM requires pow2, square textures, at least 64x64 */
	width = max(next_pow2(ALIGN_UP(width, 64)), next_pow2(ALIGN_UP(height, 64)));
	height = width;

	fbo->base.width = width;
	fbo->base.height = height;

	glGenFramebuffers(1, &fbo->base.handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->base.handle);
	glGenTextures(1, &fbo->base.texture);
	glBindTexture(GL_TEXTURE_2D, fbo->base.texture);

	vcsm_info.width = width;
	vcsm_info.height = height;
	fbo->img = eglCreateImageKHR(pint->get_egl_display(pint), EGL_NO_CONTEXT,
			EGL_IMAGE_BRCM_VCSM, &vcsm_info, NULL);
	if (fbo->img == EGL_NO_IMAGE_KHR || vcsm_info.vcsm_handle == 0) {
		fprintf(stderr, "Failed to create EGL VCSM image\n");
		goto fail;
	}
	fbo->vcsm_handle = vcsm_info.vcsm_handle;

	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, fbo->img);
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
	if (fbo->img != EGL_NO_IMAGE_KHR) {
		eglDestroyImageKHR(pint->get_egl_display(pint), fbo->img);
	}
	glDeleteTextures(1, &fbo->base.texture);
	glDeleteFramebuffers(1, &fbo->base.handle);

	free(fbo);
	return NULL;
}

char *shared_fbo_map(struct fbo *base, int32_t *stride)
{
	struct vcsm_fbo *fbo = (struct vcsm_fbo *)base;
	fbo->map = (char *)vcsm_lock(fbo->vcsm_handle);
	if (!fbo->map) {
		return NULL;
	}

	if (stride) {
		*stride = base->width * 4;
	}

	return fbo->map;
}

void shared_fbo_unmap(struct fbo *base)
{
	struct vcsm_fbo *fbo = (struct vcsm_fbo *)base;
	vcsm_unlock_ptr(fbo->map);
	fbo->map = NULL;
}
