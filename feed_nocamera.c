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
	glDeleteTextures(1, &feed->ytex);

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

	tex = texture_load("texture.pnm");
	if (!tex) {
		fprintf(stderr, "Failed to get texture\n");
		return NULL;
	}

	glGenTextures(1, &feed->ytex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, feed->ytex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->width, tex->height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	free(tex->data);
	free(tex);

	feed->terminate = terminate;
	feed->dequeue = dequeue;
	feed->queue = queue;

	return feed;
}
