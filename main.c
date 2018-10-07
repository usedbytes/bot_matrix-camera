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

struct drawcall *draw_fbo_drawcall(struct viewport *vp, struct fbo *fbo)
{
	int ret;
	ret = shader_load_compile_link("vertex_shader.glsl", "quad_fs.glsl");
	check(ret >= 0);

	struct drawcall *dc = drawcall_create(ret);
	check(dc);

	/*
	 * The image should fill the bottom-left of the texture, so grab
	 * the appropriate part and map to the full NDC range
	 */
	float max_x_uv = ((float)MATRIX_W / (float)fbo->width);
	float max_y_uv = ((float)MATRIX_H / (float)fbo->height);
	const GLfloat quad[] = {
		-1.0f,  -1.0f,      0.0f,     0.0f,
		 1.0f,  -1.0f,  max_x_uv,     0.0f,
		-1.0f,   1.0f,      0.0f, max_y_uv,
		 1.0f,   1.0f,  max_x_uv, max_y_uv,
	};
	const GLshort idx[] = {
		0, 1, 2, 3,
	};

	/*
	 * Everything is rendered into the FBO flipped vertically, because
	 * GL co-ordinates are the opposite of the raster scan expected
	 * by the display. So, when drawing the FBO for "looking at"
	 * we should flip Y to make it the right-way-up
	 */
	static const GLfloat mvp[] = {
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  -1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f,
	};

	drawcall_set_vertex_data(dc, quad, sizeof(quad));
	drawcall_set_indices(dc, idx, sizeof(idx), sizeof(idx) / sizeof(idx[0]));

	drawcall_set_attribute(dc, "position", 2, sizeof(quad[0]) * 4, (GLvoid *)0);
	drawcall_set_attribute(dc, "tc", 2, sizeof(quad[0]) * 4, (GLvoid *)(sizeof(quad[0]) * 2));

	drawcall_set_mvp(dc, mvp);
	drawcall_set_texture(dc, "tex", GL_TEXTURE_2D, fbo->texture);
	drawcall_set_viewport(dc, vp->x, vp->y, vp->w, vp->h);

	return dc;
}

static struct fbo *create_fbo(uint32_t width, uint32_t height, GLuint texture)
{
	struct fbo *fbo = malloc(sizeof(*fbo));
	if (!fbo) {
		return NULL;
	}

	glGenFramebuffers(1, &fbo->handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle);

	if (!texture) {
		glGenTextures(1, &fbo->texture);
	}
	glBindTexture(GL_TEXTURE_2D, fbo->texture);

	fbo->width = width;
	fbo->height = height;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo->texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "Framebuffer not complete\n");
		goto fail;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return fbo;

fail:
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (!texture)
		glDeleteTextures(1, &fbo->texture);
	glDeleteFramebuffers(1, &fbo->handle);
	free(fbo);
	return NULL;
}

int main(int argc, char *argv[]) {
	int i;
	struct timespec a, b;
	struct pint *pint = pint_initialise(WIDTH, HEIGHT);
	check(pint);

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
			if (ret != 0 && ret != -EAGAIN) {
				printf("Ret: %d\n", ret);
			}
			uint32_t tmp = MATRIX_W;
			comm_send(comm, 0, 4, (uint8_t *)&tmp);
			tmp = MATRIX_H;
			comm_send(comm, 0, 4, (uint8_t *)&tmp);
			comm_send(comm, 0, MATRIX_W * MATRIX_H * 4, (uint8_t *)ptr);
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
