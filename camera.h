/* Copyright 2017 Brian Starkey <stark3y@gmail.com> */

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "EGL/egl.h"

struct camera;
struct camera_buffer {
	EGLClientBuffer egl_buf;

	/* Opaque handle - don't touch! */
	void *hnd;
};

/* Wait for a frame to be available, and return it */
struct camera_buffer *camera_dequeue_buffer(struct camera *camera);
/* Return a buffer once it's finished with */
void camera_queue_buffer(struct camera *camera, struct camera_buffer *buf);

struct camera *camera_init(uint32_t width, uint32_t height, unsigned int fps);
void camera_exit(struct camera *camera);
