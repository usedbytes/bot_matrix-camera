
#ifndef __SHARED_FBO_H__
#define __SHARED_FBO_H__
#include <stdint.h>

#include "pint.h"
#include "types.h"

struct fbo *shared_fbo_create(struct pint *pint, uint32_t width, uint32_t height);
char *shared_fbo_map(struct fbo *fbo, int32_t *stride);
void shared_fbo_unmap(struct fbo *fbo);

#endif /* __SHARED_FBO_H__ */
