/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 *
 * With thanks to Ciro Santilli for his example here:
 * https://github.com/cirosantilli/cpp-cheat/blob/master/opengl/gles/triangle.c
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <linux/spi/spidev.h>

#include <pam.h>

#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "compositor.h"
#include "campipe.h"
#include "../libcomm/comm.h"
#include "font.h"
#include "pint.h"
#include "shader.h"
#include "texture.h"
#include "list.h"
#include "drawcall.h"
#include "shared_fbo.h"
#if USE_SHARED_FBO
#include "interface/vcsm/user-vcsm.h"
#endif

#include "EGL/egl.h"

#define check(_cond) { if (!(_cond)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, strerror(errno)); exit(EXIT_FAILURE); }}

#define WIDTH 480
#define HEIGHT 640
#define MESHPOINTS 32
#define MATRIX_W 128
#define MATRIX_H 128

volatile bool should_exit = 0;

void intHandler(int dummy) {
	printf("Caught signal.\n");
	should_exit = 1;
}

long elapsed_nanos(struct timespec a, struct timespec b)
{
	unsigned int sec = b.tv_sec - a.tv_sec;
	long nanos = b.tv_nsec - a.tv_nsec;

	return nanos + (1000000000 * sec);
}

int main(int argc, char *argv[]) {
	int i;
	struct timespec a, b;
	struct pint *pint = pint_initialise(WIDTH, HEIGHT);
	check(pint);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, intHandler);

	pm_init(argv[0], 0);

	printf("GL_VERSION  : %s\n", glGetString(GL_VERSION) );
	printf("GL_RENDERER : %s\n", glGetString(GL_RENDERER) );

	struct comm *comm = comm_init_tcp(9876);
	check(comm);

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glViewport(0, 0, WIDTH, HEIGHT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	/* For rendering into "the" FBO */
	struct fbo *fbo = shared_fbo_create(pint, MATRIX_W, MATRIX_H);
	struct compositor *fbo_cmp = compositor_create(fbo);
	check(fbo_cmp);
	struct viewport vp = (struct viewport){ 0, 0, MATRIX_W, MATRIX_H };
	compositor_set_viewport(fbo_cmp, &vp);

	/* For composing things onto the screen */
	struct fbo screen = {
		.width = WIDTH,
		.height = HEIGHT,
	};
	struct compositor *screen_cmp = compositor_create(&screen);
	struct layer *llayer = compositor_create_layer(screen_cmp);
	layer_set_texture(llayer, fbo->texture);
	layer_set_display_rect(llayer, 0, 0, 0.5, 0.5);
	/*
	 * When rendering to the screen, flip Y and scale up to -1:1
	 * FIXME: Instead of doubling, we should have source crop support
	 */
	const GLfloat flipy_double[] = {
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  -1.0f,  0.0f, 0.0f,
		0.0f,  0.0f,  1.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f,
	};
	layer_set_transform(llayer, flipy_double);

	/* Camera is a layer on the FBO compositor */
	struct campipe *cp = campipe_init(pint, argc == 2 ? argv[1] : NULL);
	check(cp);
	struct campipe_output *op1 = campipe_output_create(cp, MATRIX_W, MATRIX_H, true);
	check(op1);
	struct layer *camlayer = compositor_create_layer(fbo_cmp);
	layer_set_texture(camlayer, campipe_output_get_texture(op1));
	layer_set_display_rect(camlayer, 0, 0, 1.0, 1.0);

	clock_gettime(CLOCK_MONOTONIC, &a);
	while(!pint->should_end(pint)) {
		i = campipe_dequeue(cp);
		if (i != 0) {
			fprintf(stderr, "Failed dequeueing\n");
			break;
		}

		glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		/* Draw camera to the FBO */
		compositor_draw(fbo_cmp);

		/* Draw FBO to the screen */
		compositor_draw(screen_cmp);

		pint->swap_buffers(pint);
		glFinish();

		char *ptr = shared_fbo_map(fbo, NULL);
		if (ptr) {
			int ret = comm_send(comm, 0x11, 8 + MATRIX_W * MATRIX_H * 4, NULL);

			uint32_t tmp = MATRIX_W;
			if (!ret) {
				ret = comm_send(comm, 0, 4, (uint8_t *)&tmp);
			}
			tmp = MATRIX_H;
			if (!ret) {
				ret = comm_send(comm, 0, 4, (uint8_t *)&tmp);
			}
			if (!ret) {
				ret = comm_send(comm, 0, MATRIX_W * MATRIX_H * 4, (uint8_t *)ptr);
			}
			shared_fbo_unmap(fbo);
		}

		campipe_queue(cp);

		clock_gettime(CLOCK_MONOTONIC, &b);
		if (a.tv_sec != b.tv_sec) {
			long time = elapsed_nanos(a, b);
			printf("%.3f fps\n", 1000000000.0 / (float)time);
		}
		a = b;
	}

fail:
	campipe_exit(cp);
	pint->terminate(pint);
#if USE_SHARED_FBO
	vcsm_exit();
#endif

	return EXIT_SUCCESS;
}
