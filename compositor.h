/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#include "types.h"
#include "feed.h"

struct compositor;
struct layer;

struct compositor *compositor_create(struct fbo *framebuffer, struct feed *feed);
void compositor_set_viewport(struct compositor *cmp, struct viewport *vp);
void compositor_draw(struct compositor *cmp);
struct layer *compositor_create_layer(struct compositor *cmp);
void layer_set_texture(struct layer *layer, GLint handle);
void layer_set_display_rect(struct layer *layer, float x, float y, float w, float h);
void layer_set_mvp(struct layer *layer, const GLfloat *mvp);
