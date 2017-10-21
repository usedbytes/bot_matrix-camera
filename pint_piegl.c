/*
 * Based on the vc examples:
 * Copyright (c) 2012, Broadcom Europe Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "bcm_host.h"
#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include "pint.h"

extern volatile bool should_exit;

#define PIEGL_PINT(_pint) ((struct piegl_pint *)_pint)
struct piegl_pint {
	struct pint base;

	uint32_t screen_width;
	uint32_t screen_height;

	// OpenGL|ES objects
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
};

static void swap_buffers(struct pint *p)
{
	struct piegl_pint *pint = PIEGL_PINT(p);

	eglSwapBuffers(pint->display, pint->surface);
}

static bool should_end(struct pint *p)
{
	return should_exit;
}

static void terminate(struct pint *p)
{
	struct piegl_pint *pint = PIEGL_PINT(p);

	DISPMANX_UPDATE_HANDLE_T dispman_update;
	int s;

	eglDestroySurface(pint->display, pint->surface);

	dispman_update = vc_dispmanx_update_start( 0 );
	s = vc_dispmanx_element_remove(dispman_update, pint->dispman_element);
	assert(s == 0);
	vc_dispmanx_update_submit_sync( dispman_update );
	s = vc_dispmanx_display_close(pint->dispman_display);
	assert (s == 0);

	// Release OpenGL resources
	eglMakeCurrent(pint->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(pint->display, pint->context);
	eglTerminate(pint->display);
}

static EGLDisplay get_egl_display(struct pint *p)
{
	struct piegl_pint *pint = PIEGL_PINT(p);

	return pint->display;
}

struct pint *pint_initialise(uint32_t width, uint32_t height)
{
	int32_t success = 0;
	EGLBoolean result;
	EGLint num_config;
	static EGL_DISPMANX_WINDOW_T nativewindow;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

	EGLConfig config;
	struct piegl_pint *pint = calloc(1, sizeof(*pint));
	if (!pint)
		return NULL;

	bcm_host_init();

	pint->base.swap_buffers = swap_buffers;
	pint->base.terminate = terminate;
	pint->base.should_end = should_end;
	pint->base.get_egl_display = get_egl_display;

	// get an EGL display connection
	pint->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(pint->display!=EGL_NO_DISPLAY);

	// initialize the EGL display connection
	result = eglInitialize(pint->display, NULL, NULL);
	assert(EGL_FALSE != result);

	// get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(pint->display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	pint->context = eglCreateContext(pint->display, config, EGL_NO_CONTEXT, contextAttribs);
	assert(pint->context!=EGL_NO_CONTEXT);

	// create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */, &pint->screen_width, &pint->screen_height);
	assert( success >= 0 );

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = pint->screen_width;
	dst_rect.height = pint->screen_height;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = pint->screen_width << 16;
	src_rect.height = pint->screen_height << 16;

	pint->dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
	dispman_update = vc_dispmanx_update_start( 0 );

	pint->dispman_element = vc_dispmanx_element_add ( dispman_update, pint->dispman_display,
			0/*layer*/, &dst_rect, 0/*src*/,
			&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

	nativewindow.element = pint->dispman_element;
	nativewindow.width = pint->screen_width;
	nativewindow.height = pint->screen_height;
	vc_dispmanx_update_submit_sync( dispman_update );

	pint->surface = eglCreateWindowSurface( pint->display, config, &nativewindow, NULL );
	assert(pint->surface != EGL_NO_SURFACE);

	// connect the context to the surface
	result = eglMakeCurrent(pint->display, pint->surface, pint->surface, pint->context);
	assert(EGL_FALSE != result);

	return (struct pint *)pint;
}
