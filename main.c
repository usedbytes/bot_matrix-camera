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
#include "font.h"
#include "pint.h"
#include "shader.h"
#include "texture.h"
#include "list.h"
#include "drawcall.h"
#if USE_SHARED_FBO
#include "interface/vcsm/user-vcsm.h"
#include "shared_fbo.h"
#endif

#include "EGL/egl.h"

#define check(_cond) { if (!(_cond)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, strerror(errno)); exit(EXIT_FAILURE); }}

#define WIDTH 640
#define HEIGHT 480
#define MESHPOINTS 32
#define MATRIX_W 32
#define MATRIX_H 32

extern int draw_rect(int fd, unsigned int x, unsigned int y, unsigned int w,
	      unsigned int h, char *data, unsigned int stride, bool flip);

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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbo->width, fbo->height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
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

#if USE_SHARED_FBO
struct foo {
	int fd;
	char *data;
};

void dump_hex(unsigned int len, char *data) {
	unsigned int i;

	for (i = 0; i < len; i++) {
		printf("%02x", data[i]);
	}
	printf("\n");
}

void *async(void *p) {
	struct foo *foo = p;

	draw_rect(foo->fd, 0, 0, 32, 16, foo->data, 64 * 4, false);
	draw_rect(foo->fd, 0, 16, 32, 16, foo->data + (64 * 4 * 16), 64 * 4, true);

	/*
	int i;
	char *d = foo->data;
	printf("=====================\n");
	for (i = 0; i < 32; i++) {
		dump_hex(32 * 4, d);
		d += 64*4;
	}
	printf("=====================\n");
	*/

	return NULL;
}
#endif

float centre_align(struct font *font, const char *str, float size, int pixel_width)
{
	float width = font_calculate_width(font, str, size);
	float x = (width / 2);

	float pixel = 2.0f / pixel_width;
	float rem = fmod(x, pixel);
	if (rem < pixel / 2) {
		x -= rem;
	} else {
		x += (pixel - rem);
	}

	return -x;
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

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glViewport(0, 0, WIDTH, HEIGHT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	struct campipe *cp = campipe_init(pint, argc == 2 ? argv[1] : NULL);
	check(cp);

#if USE_SHARED_FBO
	printf("Using shared FBO\n");

	i = vcsm_init();
	fprintf(stderr, "vcsm_init(): %d\n", i);

	uint32_t spd = 3300000;
	struct fbo *fbo = shared_fbo_create(pint, MATRIX_W, MATRIX_H);
	static char buf[64 * 4 * 32];
	pthread_t id;
	bool have_thread = false;

	int fd = open("/dev/spidev1.2", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed opening spidev: %s\n", strerror(errno));
		return 1;
	}
	i = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spd);
	if (i != 0) {
		fprintf(stderr, "Failed setting spidev WR: %s\n", strerror(errno));
		goto fail;
	}

	struct foo foo = {
		.fd = fd,
		.data = buf,
	};
#else
	struct fbo *fbo = create_fbo(MATRIX_W * 2, MATRIX_H * 2, 0);
#endif

	struct fbo screen = {
		.width = WIDTH,
		.height = HEIGHT,
	};

	struct campipe_output *op1 = campipe_output_create(cp, 32, 32, true);
	check(op1);

	struct campipe_output *op2 = campipe_output_create(cp, 32, 32, false);
	check(op2);

	struct compositor *cmp = compositor_create(fbo);
	struct viewport vp = (struct viewport){ 0, 0, MATRIX_W, MATRIX_H };
	compositor_set_viewport(cmp, &vp);
	check(cmp);

	struct layer *camlayer = compositor_create_layer(cmp);
	layer_set_texture(camlayer, campipe_output_get_texture(op1));
	layer_set_display_rect(camlayer, 0, 0, 1.0, 1.0);

	struct texture *bmt = texture_load("bot_m.png");
	check(bmt);

	struct layer *layer = compositor_create_layer(cmp);
	layer_set_texture(layer, bmt->handle);
	texture_set_filter(bmt, GL_NEAREST);
	layer_set_display_rect(layer, 0, 0, 1.0, 1.0);

	struct compositor *screencmp = compositor_create(&screen);
	struct layer *llayer = compositor_create_layer(screencmp);
	layer_set_texture(llayer, fbo->texture);
	layer_set_display_rect(llayer, 0, 0, 1.0, 1.0);
	/*
	 * When rendering to the screen, flip Y and scale up to -1:1
	 * FIXME: Instead of doubling, we should have source crop support
	 */
	const GLfloat flipy_double[] = {
		2.0f,  0.0f,  0.0f,  1.0f,
		0.0f,  -2.0f,  0.0f, -1.0f,
		0.0f,  0.0f,  1.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f,
	};
	layer_set_transform(llayer, flipy_double);

	struct fbo *font_fbo = create_fbo(MATRIX_W, MATRIX_H, 0);
	check(font_fbo);
	struct font *f = font_load("font.png", " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`{|}~\x7f");
	struct drawcall *font_dc = create_font_drawcall(f, MATRIX_W, MATRIX_H);
	drawcall_set_fbo(font_dc, font_fbo);

	float x = centre_align(f, "BOT", 18.0f / 32.0f, MATRIX_W);
	struct element_array *arr = font_calculate(f, "BOT", x, 0, 18.0f / 32.0f);
	x = centre_align(f, "MATRIX", 18.0f / 32.0f, MATRIX_W);
	struct element_array *arrb = font_calculate(f, "MATRIX", x, 0.5, 18.0f / 32.0f);
	element_array_append(arr, arrb);

	drawcall_set_vertex_data(font_dc, arr->vertices, sizeof(*arr->vertices) * arr->nverts);
	drawcall_set_indices(font_dc, arr->indices, sizeof(*arr->indices) * arr->nidx, arr->nidx);
	drawcall_set_viewport(font_dc, 0, 0, MATRIX_W, MATRIX_H);
	element_array_free(arr);

	struct texture font_tex = {
		.handle = font_fbo->texture,
	};
	struct layer *font_layer = compositor_create_layer(cmp);
	layer_set_texture(font_layer, font_fbo->texture);
	texture_set_filter(&font_tex, GL_NEAREST);
	layer_set_display_rect(font_layer, 0, 0, 1.0, 1.0);
	static const GLfloat flipy[] = {
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  -1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f,
	};
	layer_set_transform(font_layer, flipy);

	clock_gettime(CLOCK_MONOTONIC, &a);
	while(!pint->should_end(pint)) {
		i = campipe_dequeue(cp);
		if (i != 0) {
			fprintf(stderr, "Failed dequeueing\n");
			break;
		}

		compositor_draw(cmp);

		glClear(GL_COLOR_BUFFER_BIT);
		compositor_draw(screencmp);

		/* Draw the text */
		glBindFramebuffer(GL_FRAMEBUFFER, font_dc->fbo->handle);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		drawcall_draw(font_dc);

		pint->swap_buffers(pint);
		glFinish();

#if USE_SHARED_FBO
		{
			int32_t stride;
			char *map = shared_fbo_map(fbo, &stride);
			if (!map) {
				fprintf(stderr, "Failed to map shared buffer\n");
				break;
			}

			if (have_thread) {
				pthread_join(id, NULL);
			}
			memcpy(buf, map, stride * 32);
			i = pthread_create(&id, NULL, async, &foo);
			if (i != 0) {
				fprintf(stderr, "Failed creating thread: %s\n", strerror(errno));
			}
			have_thread = true;

			shared_fbo_unmap(fbo);
		}
#endif

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
