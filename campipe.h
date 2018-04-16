/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#ifndef __CAMPIPE_H__
#define __CAMPIPE_H__
#include <stdint.h>
#include <stdbool.h>

#include "pint.h"

struct campipe;
struct campipe_output;

struct campipe *campipe_init(struct pint *pint, const char *ldc_file);
void campipe_exit(struct campipe *cp);
struct campipe_output *campipe_output_create(struct campipe *cp, int width, int height, bool ldc);
// campipe_output_destroy();
int campipe_output_get_texture(struct campipe_output *op);

int campipe_dequeue(struct campipe *cp);
void campipe_queue(struct campipe *cp);

#endif /* __CAMPIPE_H__ */
