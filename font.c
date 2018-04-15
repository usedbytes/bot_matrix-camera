
#include <stdio.h>
#include <string.h>
#include "texture.h"
#include "drawcall.h"
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

void font_calculate(struct font *font, struct drawcall *dc, const char *str, float size)
{
	int i = 0;
	int c;
	const char *p = str;
	struct glyph *g;
	float *vertices = calloc(strlen(str) * 4 * 4, sizeof(*vertices));
	GLshort *indices = calloc(strlen(str) * 6, sizeof(*indices));
	float *vp;
	GLshort *ip;
	size_t vsize = sizeof(*vertices) * strlen(str) * 4 * 4;
	size_t isize = sizeof(*indices) * strlen(str) * 6;

	float x = -1.0f, y = 1.0f - size, e = size / font->height;

	while (*p) {
		c = *p;
		if (!font->glyphs[c].width) {
			c = 127;
		}
		g = &font->glyphs[c];

		float ndc_width = e * (g->width);

		vp = &vertices[i * 16];
		ip = &indices[i * 6];

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

	drawcall_set_vertex_data(dc, vertices, sizeof(*vertices) * vsize);
	drawcall_set_indices(dc, indices, sizeof(*indices) * isize, strlen(str) * 6);
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
