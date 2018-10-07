/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#include <stdio.h>

#include <GLES2/gl2.h>
#include <GLES/gl.h>

#include "campipe.h"
#include "drawcall.h"
#include "feed.h"
#include "list.h"
#include "mesh.h"
#include "shader.h"
#include "types.h"

struct campipe {
	struct feed *feed;
	struct list_head outputs;

	/* Lens distortion correction */
	GLfloat *ldc_mesh;
	GLshort *ldc_indices;
	unsigned int nverts;
	unsigned int nindices;
};

struct campipe_output {
	struct list_head list;
	struct campipe *cp;
	struct drawcall *dc;
	struct fbo *fbo;
};

static void output_update_textures(struct campipe_output *op);

static int init_ldc_mesh(struct campipe *cp, const char *file)
{
	int xpoints, ypoints;

	if (file) {
		cp->ldc_mesh = mesh_build_from_file(file, &cp->nverts, &xpoints, &ypoints);
		if (!cp->ldc_mesh) {
			return -1;
		}
	} else {
		xpoints = ypoints = 2;
		cp->ldc_mesh = mesh_build(2, 2, NULL, &cp->nverts);
		if (!cp->ldc_mesh) {
			return -1;
		}
	}

	cp->ldc_indices = mesh_build_indices(xpoints, ypoints, &cp->nindices);
	if (!cp->ldc_indices) {
		free(cp->ldc_mesh);
		return -1;
	}

	return 0;
}

int campipe_enable(struct campipe *cp)
{
	return cp->feed->enable(cp->feed);
}

int campipe_disable(struct campipe *cp)
{
	return cp->feed->disable(cp->feed);
}

struct campipe *campipe_init(struct pint *pint, const char *ldc_file)
{
	int ret;
	struct campipe *cp = calloc(1, sizeof(*cp));
	if (!cp) {
		return NULL;
	}

	cp->feed = feed_init(pint);
	if (!cp->feed) {
		goto fail;
	}

	list_init(&cp->outputs);

	ret = init_ldc_mesh(cp, ldc_file);
	if (ret) {
		goto fail;
	}

	return cp;

fail:
	campipe_exit(cp);
	return NULL;
}

int campipe_dequeue(struct campipe *cp)
{
	int ret = cp->feed->dequeue(cp->feed);
	if (ret != 0) {
		fprintf(stderr, "Failed dequeueing\n");
		return ret;
	}

	struct list_head *l = cp->outputs.next;
	while (l != &cp->outputs) {
		struct campipe_output *op = (struct campipe_output *)l;
		output_update_textures(op);

		drawcall_draw(op->dc);

		l = l->next;
	}

	return 0;
}

void campipe_queue(struct campipe *cp)
{
	cp->feed->queue(cp->feed);
}

void campipe_exit(struct campipe *cp)
{
	cp->feed->terminate(cp->feed);
	free(cp->ldc_mesh);
	free(cp->ldc_indices);
	free(cp);
}

static void output_update_textures(struct campipe_output *op)
{
	drawcall_set_texture(op->dc, "ytex", op->cp->feed->ytex.bind, op->cp->feed->ytex.handle);
	drawcall_set_texture(op->dc, "utex", op->cp->feed->utex.bind, op->cp->feed->utex.handle);
	drawcall_set_texture(op->dc, "vtex", op->cp->feed->vtex.bind, op->cp->feed->vtex.handle);
}

static struct fbo *fbo_create(uint32_t width, uint32_t height)
{
	struct fbo *fbo = malloc(sizeof(*fbo));
	if (!fbo) {
		return NULL;
	}

	glGenFramebuffers(1, &fbo->handle);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle);

	glGenTextures(1, &fbo->texture);
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
	glDeleteTextures(1, &fbo->texture);
	glDeleteFramebuffers(1, &fbo->handle);
	free(fbo);
	return NULL;
}

static void campipe_output_set_ldc(struct campipe_output *op, bool enable)
{
	if (enable) {
		drawcall_set_attribute(op->dc, "tc", 2, sizeof(op->cp->ldc_mesh[0]) * 4, (GLvoid *)(sizeof(op->cp->ldc_mesh[0]) * 2));
	} else {
		drawcall_set_attribute(op->dc, "tc", 2, sizeof(op->cp->ldc_mesh[0]) * 4, (GLvoid *)0);
	}
}

struct campipe_output *campipe_output_create(struct campipe *cp, int width, int height, bool ldc)
{
	/*
	 * Simple MVP matrix which flips the Y axis (so 0,0 is top left) and
	 * scales/translates everything so that on-screen points are 0-1
	 */
	static const GLfloat mvp[] = {
		 2.0f,  0.0f,  0.0f, -1.0f,
		 0.0f,  2.0f,  0.0f,  -1.0f,
		 0.0f,  0.0f,  0.0f,   0.0f,
		 0.0f,  0.0f,  0.0f,   1.0f,
	};

	int shader;
	struct campipe_output *op = calloc(1, sizeof(*op));
	if (!op) {
		return NULL;
	}

	op->cp = cp;

	op->fbo = fbo_create(width, height);
	if (!op->fbo) {
		goto fail;
	}

	shader = shader_load_compile_link("vertex_shader.glsl", FRAGMENT_SHADER);
	if (shader <= 0) {
		goto fail;
	}

	op->dc = drawcall_create(shader);
	if (!op->dc) {
		goto fail;
	}

	drawcall_set_vertex_data(op->dc, cp->ldc_mesh, cp->nverts * sizeof(*cp->ldc_mesh));
	drawcall_set_indices(op->dc, cp->ldc_indices, cp->nindices * sizeof(*cp->ldc_mesh), cp->nindices);

	drawcall_set_attribute(op->dc, "position", 2, sizeof(cp->ldc_mesh[0]) * 4, (GLvoid *)0);
	campipe_output_set_ldc(op, ldc);

	drawcall_set_mvp(op->dc, mvp);
	drawcall_set_viewport(op->dc, 0, 0, width, height);
	drawcall_set_fbo(op->dc, op->fbo);

	output_update_textures(op);

	list_add_end(&cp->outputs, (struct list_head *)op);

	return op;

fail:
	// Destroy FBO
	// Destroy Shader
	free(op);
	return NULL;
}

int campipe_output_get_texture(struct campipe_output *op)
{
	return op->fbo->texture;
}
