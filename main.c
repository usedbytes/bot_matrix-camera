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
#include "pint.h"
#include "shader.h"
#include "texture.h"
#include "list.h"
#include "mesh.h"
#include "feed.h"
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

struct mesh *mesh;

void intHandler(int dummy) {
	printf("Caught signal.\n");
	should_exit = 1;
}

struct mesh {
	GLfloat *mesh;
	unsigned int nverts;
	GLuint mhandle;

	GLshort *indices;
	unsigned int nindices;
	GLuint ihandle;
};

struct mesh *get_mesh(const char *file)
{
	int xpoints, ypoints;
	struct mesh *mesh = calloc(1, sizeof(*mesh));
	if (!mesh) {
		return NULL;
	}

	if (file) {
		mesh->mesh = mesh_build_from_file(file, &mesh->nverts, &xpoints, &ypoints);
		if (!mesh->mesh) {
			free(mesh);
			return NULL;
		}
	} else {
		xpoints = ypoints = MESHPOINTS;
		mesh->mesh = mesh_build(xpoints, ypoints, NULL, &mesh->nverts);
		if (!mesh->mesh) {
			free(mesh);
			return NULL;
		}
	}


	glGenBuffers(1, &mesh->mhandle);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->mhandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(mesh->mesh[0]) * mesh->nverts, mesh->mesh, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	mesh->indices = mesh_build_indices(xpoints, ypoints, &mesh->nindices);
	if (!mesh->indices) {
		free(mesh->mesh);
		free(mesh);
		return NULL;
	}

	glGenBuffers(1, &mesh->ihandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ihandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mesh->indices[0]) * mesh->nindices, mesh->indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return mesh;
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

struct drawcall *get_camera_drawcall(const char *vs_fname, const char *fs_fname, struct viewport *vp, struct fbo *fbo)
{
	/*
	 * Simple MVP matrix which flips the Y axis (so 0,0 is top left) and
	 * scales/translates everything so that on-screen points are 0-1
	 */
	static const GLfloat mvp[] = {
		 0.0f,  2.0f,  0.0f,  -1.0f,
		 2.0f,  0.0f,  0.0f,  -1.0f,
		 0.0f,  0.0f,  0.0f,   0.0f,
		 0.0f,  0.0f,  0.0f,   1.0f,
	};

	GLint posLoc, tcLoc, mvpLoc, texLoc;
	struct drawcall *dc = calloc(1, sizeof(*dc));
	int ret;

	if (!vp) {
		return NULL;
	}

	dc->yidx = dc->uidx = dc->vidx = -1;

	ret = shader_load_compile_link(vs_fname, fs_fname);
	check(ret >= 0);
	dc->shader_program = ret;

	glUseProgram(dc->shader_program);

	posLoc = glGetAttribLocation(dc->shader_program, "position");
	tcLoc = glGetAttribLocation(dc->shader_program, "tc");
	mvpLoc = glGetUniformLocation(dc->shader_program, "mvp");

	dc->n_attributes = 2;
	dc->attributes[0] = (struct attr){
		.loc = posLoc,
		.size = 2,
		.stride = sizeof(mesh->mesh[0]) * 4,
		.ptr = (GLvoid *)0,
	};
	dc->attributes[1] = (struct attr){
		.loc = tcLoc,
		.size = 2,
		.stride = sizeof(mesh->mesh[0]) * 4,
		.ptr = (GLvoid *)(sizeof(mesh->mesh[0]) * 2),
	};

	texLoc = glGetUniformLocation(dc->shader_program, "ytex");
	glUniform1i(texLoc, 0);
	texLoc = glGetUniformLocation(dc->shader_program, "utex");
	glUniform1i(texLoc, 1);
	texLoc = glGetUniformLocation(dc->shader_program, "vtex");
	glUniform1i(texLoc, 2);
	glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp);

	dc->n_buffers = 2;
	dc->buffers[0] = (struct bind){ .bind = GL_ARRAY_BUFFER, .handle = mesh->mhandle };
	dc->buffers[1] = (struct bind){ .bind = GL_ELEMENT_ARRAY_BUFFER, .handle = mesh->ihandle };
	dc->n_indices = mesh->nindices;

	dc->n_textures = 3;
	// TEXTURE0,1,2 is Y,U,V
	dc->yidx = 0;
	dc->uidx = 1;
	dc->vidx = 2;

	dc->viewport = *vp;

	dc->fbo = fbo;
	dc->draw = draw_elements;

	glUseProgram(0);

	return dc;
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

int main(int argc, char *argv[]) {
	int i;
	struct timespec a, b;
	struct pint *pint = pint_initialise(WIDTH, HEIGHT);
	check(pint);

	signal(SIGINT, intHandler);

	pm_init(argv[0], 0);

	mesh = get_mesh(argc == 2 ? argv[1] : NULL);
	check(mesh);

	printf("GL_VERSION  : %s\n", glGetString(GL_VERSION) );
	printf("GL_RENDERER : %s\n", glGetString(GL_RENDERER) );

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glViewport(0, 0, WIDTH, HEIGHT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	struct feed *feed = feed_init(pint);
	check(feed);

	struct drawcall *dc, *last_dc;
	struct list_head drawcalls;
	list_init(&drawcalls);

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

	struct viewport vp = { 0, HEIGHT / 2, WIDTH / 2, HEIGHT / 2 };
	dc = get_camera_drawcall("vertex_shader.glsl", "y_shader.glsl", &vp, NULL);
	check(dc);
	list_add_end(&drawcalls, &dc->list);

	vp = (struct viewport){ 0, 0, WIDTH / 2, HEIGHT / 2 };
	dc = get_camera_drawcall("vertex_shader.glsl", "u_shader.glsl", &vp, NULL);
	check(dc);
	list_add_end(&drawcalls, &dc->list);

	vp = (struct viewport){ WIDTH / 2, 0, WIDTH / 2, HEIGHT / 2 };
	dc = get_camera_drawcall("vertex_shader.glsl", "v_shader.glsl", &vp, NULL);
	check(dc);
	list_add_end(&drawcalls, &dc->list);

	/*
	 * Render into the bottom-left of the FBO (in GL co-ordinates). This is
	 * actually the top-left when we read it via VCSM (or, said differently,
	 * the first row of bytes in the VCSM buffer corresponds to the bottom
	 * row of pixels in GL co-ordinate space)
	 */
	vp = (struct viewport){ 0, 0, MATRIX_W, MATRIX_H };
	dc = get_camera_drawcall("vertex_shader.glsl", FRAGMENT_SHADER, &vp, fbo);
	check(dc);
	list_add_end(&drawcalls, &dc->list);

	/* When drawing the FBO to the screen, we flip it with the MVP matrix */
	vp = (struct viewport){ WIDTH / 2, HEIGHT / 2, WIDTH / 2, HEIGHT / 2 };
	last_dc = draw_fbo_drawcall(&vp, fbo);
	check(last_dc);

	struct compositor *cmp = compositor_create(fbo, feed);
	vp = (struct viewport){ 0, 0, MATRIX_W, MATRIX_H };
	compositor_set_viewport(cmp, &vp);
	check(cmp);

	struct texture *bmt = texture_load("bot_m.png");
	check(bmt);

	struct layer *layer = compositor_create_layer(cmp);
	layer_set_texture(layer, bmt->handle);
	texture_set_filter(bmt, GL_NEAREST);
	layer_set_display_rect(layer, 0, 0, 1.0, 1.0);

	clock_gettime(CLOCK_MONOTONIC, &a);
	while(!pint->should_end(pint)) {
		i = feed->dequeue(feed);
		if (i != 0) {
			fprintf(stderr, "Failed dequeueing\n");
			break;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		struct list_head *l = drawcalls.next;
		while (l != &drawcalls) {
			dc = (struct drawcall *)l;
			drawcall_draw(feed, dc);

			l = l->next;
		}

		compositor_draw(cmp);

		drawcall_draw(feed, last_dc);

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

		feed->queue(feed);

		clock_gettime(CLOCK_MONOTONIC, &b);
		if (a.tv_sec != b.tv_sec) {
			long time = elapsed_nanos(a, b);
			printf("%.3f fps\n", 1000000000.0 / (float)time);
		}
		a = b;
	}

fail:
	feed->terminate(feed);
	pint->terminate(pint);
#if USE_SHARED_FBO
	vcsm_exit();
#endif

	return EXIT_SUCCESS;
}
