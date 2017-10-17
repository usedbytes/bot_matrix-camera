/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#include <stdio.h>

#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#include "feed.h"
#include "texture.h"

static void terminate(struct feed *feed)
{
	glDeleteTextures(1, &feed->ytex.handle);

	free(feed);
}

static int dequeue(struct feed *f)
{
	f = NULL;
	return 0;
}

static void queue(struct feed *f)
{
	f = NULL;
	return;
}

struct feed *feed_init(struct pint *pint)
{
	struct texture *tex;
	struct feed *feed = calloc(1, sizeof(*feed));
	if (!feed)
		return NULL;

	pint = NULL;

	tex = texture_load("luma.pgm");
	if (!tex) {
		fprintf(stderr, "Failed to get texture\n");
		return NULL;
	}

	feed->ytex.bind = GL_TEXTURE_2D;
	glGenTextures(1, &feed->ytex.handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, feed->ytex.handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex->width, tex->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(tex->data);
	free(tex);

	tex = texture_load("cb.pgm");
	if (!tex) {
		fprintf(stderr, "Failed to get texture\n");
		return NULL;
	}

	feed->utex.bind = GL_TEXTURE_2D;
	glGenTextures(1, &feed->utex.handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, feed->utex.handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex->width, tex->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(tex->data);
	free(tex);

	tex = texture_load("cr.pgm");
	if (!tex) {
		fprintf(stderr, "Failed to get texture\n");
		return NULL;
	}

	feed->vtex.bind = GL_TEXTURE_2D;
	glGenTextures(1, &feed->vtex.handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, feed->vtex.handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex->width, tex->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(tex->data);
	free(tex);

	feed->terminate = terminate;
	feed->dequeue = dequeue;
	feed->queue = queue;

	return feed;
}
