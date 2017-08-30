/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#ifndef __PINT_H__
#define __PINT_H__
#include <stdbool.h>

#include "EGL/egl.h"

struct pint {
	void (*swap_buffers)(struct pint *);
	bool (*should_end)(struct pint *);
	void (*terminate)(struct pint *);
	EGLDisplay (*get_egl_display)(struct pint *);
};

extern struct pint *pint_initialise(uint32_t width, uint32_t height);

#endif /* __PINT_H__ */
