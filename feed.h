/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */

#ifndef __FEED_H__
#define __FEED_H__
#include <stdint.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

#include "pint.h"
#include "types.h"

struct feed {
	struct bind ytex, utex, vtex;

	void (*terminate)(struct feed *f);
	int (*dequeue)(struct feed *f);
	void (*queue)(struct feed *f);
	int (*disable)(struct feed *f);
	int (*enable)(struct feed *f);
};

struct feed *feed_init(struct pint *pint);

#endif /* __FEED_H__ */
