
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texture.h"
#include "drawcall.h"
#include "font.h"
#include "shader.h"

struct glyph {
	float x1, x2;
	int width;
};

struct font {
	struct texture *tex;
	int height;
	float maxy;
	struct glyph glyphs[128];
};

static void get_font_widths(struct font *font, const char *charset)
{
	int i, c, width;
	int pitch = font->tex->datalen / font->tex->height ;
	unsigned char *p = (unsigned char *)font->tex->data + (pitch * font->height);

	for (i = 0, c = 0, width = 0; i < font->tex->width; i++, width++, p += 4) {
		if ((p[0] == 0xff) && (p[1] == 0xff) && (p[2] == 0xff) && (p[3] == 0xff)) {
			if (c > strlen(charset)) {
				fprintf(stderr, "Corrupt font image. Too many width blobs\n");
			}
			printf("char %c, width: %d\n", charset[c], width);
			int ch = charset[c];
			font->glyphs[ch].width = width;
			width = 0;
			c++;
		}
	}

	if (c != strlen(charset)) {
		fprintf(stderr, "Corrupt font image. Not enough width blobs\n");
	}
}

struct font *font_load(const char *image, const char *charset)
{
	int i;
	float inc, x = 0.0f;
	struct font *font = calloc(1, sizeof(*font));
	if (!font) {
		return NULL;
	}

	font->tex = texture_load(image);
	if (!font->tex) {
		goto fail;
	}
	texture_set_filter(font->tex, GL_NEAREST);

	/* The bottom row of pixels is used to detect glyph widths */
	font->height = font->tex->height - 1;
	font->maxy = 1.0f - (1.0f / font->tex->height);

	get_font_widths(font, charset);

	inc = 1.0f / (float)(font->tex->width);
	x += inc;
	for (i = 0; i < 128; i++) {
		if (!font->glyphs[i].width) {
			continue;
		}

		font->glyphs[i].x1 = x;
		x += font->glyphs[i].width * inc;
		font->glyphs[i].x2 = x;
	}

	return font;

fail:
	free(font);
	return NULL;
}

struct element_array *element_array_alloc(size_t nverts, size_t nidx)
{
	struct element_array *a = calloc(1, sizeof(*a));

	a->nverts = nverts;
	a->vertices = calloc(nverts, sizeof(*a->vertices));
	a->nidx = nidx;
	a->indices = calloc(nidx, sizeof(*a->indices));

	return a;
}

void element_array_free(struct element_array *a)
{
	free(a->vertices);
	free(a->indices);
}

void element_array_append(struct element_array *a, struct element_array *b)
{
	size_t i, iidx;
	a->vertices = realloc(a->vertices, (a->nverts + b->nverts) * sizeof(*a->vertices));
	memcpy(&a->vertices[a->nverts], b->vertices, sizeof(*b->vertices) * b->nverts);
	a->nverts = a->nverts + b->nverts;

	iidx = (a->nidx * 4) / 6;
	for (i = 0; i < b->nidx; i++) {
		b->indices[i] += iidx;
	}
	a->indices = realloc(a->indices, (a->nidx + b->nidx) * sizeof(*a->indices));
	memcpy(&a->indices[a->nidx], b->indices, sizeof(*b->indices) * b->nidx);
	a->nidx = a->nidx + b->nidx;

	element_array_free(b);
}

struct element_array *font_calculate(struct font *font, const char *str,
			    float x, float y, float size)
{
	size_t len = strlen(str);
	struct element_array *a = element_array_alloc(len * 4 * 4, len * 6);
	int i = 0;
	int c;
	const char *p = str;
	struct glyph *g;
	float *vp;
	GLshort *ip;
	float e = size / font->height;

	printf("Calculate '%s' at %2.3f,%2.3f\n", str, x, y);

	y = y - size;

	while (*p) {
		c = *p;
		if (!font->glyphs[c].width) {
			c = 127;
		}
		g = &font->glyphs[c];

		float ndc_width = e * (g->width);

		vp = &a->vertices[i * 16];
		ip = &a->indices[i * 6];

		vp[0] = x;
		vp[1] = y + size;
		vp[2] = g->x1;
		vp[3] = font->maxy;

		vp[4] = x;
		vp[5] = y;
		vp[6] = g->x1;
		vp[7] = 0.0f;

		vp[8] = x + ndc_width;
		vp[9] = y;
		vp[10] = g->x2;
		vp[11] = 0.0f;

		vp[12] = x + ndc_width;
		vp[13] = y + size;
		vp[14] = g->x2;
		vp[15] = font->maxy;

		ip[0] = (i * 4) + 0;
		ip[1] = (i * 4) + 1;
		ip[2] = (i * 4) + 2;
		ip[3] = (i * 4) + 2;
		ip[4] = (i * 4) + 3;
		ip[5] = (i * 4) + 0;

		x += e * (g->width - 1);

		p++;
		i++;
	}

	return a;
}

float font_calculate_width(struct font *font, const char *str, float size)
{
	int c;
	const char *p = str;
	struct glyph *g;

	float x = 0.0f, e = size / font->height;

	while (*p) {
		c = *p;
		if (!font->glyphs[c].width) {
			c = 127;
		}
		g = &font->glyphs[c];

		x += e * (g->width - 1);

		p++;
	}

	x += e;

	printf("Width '%s': %2.3f\n", str, x);
	return x;
}

static void draw_triangles(struct drawcall *dc)
{
	glDrawElements(GL_TRIANGLES, dc->n_indices, GL_UNSIGNED_SHORT, 0);
}

struct drawcall *create_font_drawcall(struct font *font, int width, int height)
{
	int shader = shader_load_compile_link("compositor_vs.glsl", "compositor_fs.glsl");
	if (shader <= 0) {
		fprintf(stderr, "Couldn't get compositor shaders\n");
		return NULL;
	}

	struct drawcall *dc = drawcall_create(shader);
	if (!dc) {
		fprintf(stderr, "Couldn't get compositor shaders\n");
		return NULL;
	}

	static const GLfloat mvp[] = {
		1.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  -1.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  0.0f,
		0.0f,  0.0f,  0.0f,  1.0f,
	};

	drawcall_set_mvp(dc, mvp);
	drawcall_set_texture(dc, "tex", GL_TEXTURE_2D, font->tex->handle);
	drawcall_set_viewport(dc, 0, 0, width, height);

	drawcall_set_attribute(dc, "position", 2, sizeof(float) * 4, (GLvoid *)0);
	drawcall_set_attribute(dc, "tc", 2, sizeof(float) * 4, (GLvoid *)(sizeof(float) * 2));

	dc->draw = draw_triangles;

	return dc;
}
