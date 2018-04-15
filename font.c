
#include <stdio.h>
#include <string.h>
#include "texture.h"
#include "drawcall.h"
#include "shader.h"

const char font_widths[128] =  {
	[' '] = 5,
	['!'] = 5,
	['"'] = 7,
	['#'] = 7,
	['$'] = 7,
	['%'] = 7,
	['&'] = 8,
	['\''] = 4,
	['('] = 4,
	[')'] = 4,
	['*'] = 7,
	['+'] = 7,
	[','] = 4,
	['-'] = 5,
	['.'] = 4,
	['/'] = 6,
	['0'] = 7,
	['1'] = 7,
	['2'] = 7,
	['3'] = 7,
	['4'] = 7,
	['5'] = 7,
	['6'] = 7,
	['7'] = 7,
	['8'] = 7,
	['9'] = 7,
	[':'] = 4,
	[';'] = 4,
	['<'] = 5,
	['='] = 5,
	['>'] = 5,
	['?'] = 7,
	['@'] = 9,
	['A'] = 7,
	['B'] = 7,
	['C'] = 7,
	['D'] = 7,
	['E'] = 7,
	['F'] = 7,
	['G'] = 7,
	['H'] = 7,
	['I'] = 3,
	['J'] = 7,
	['K'] = 7,
	['L'] = 7,
	['M'] = 7,
	['N'] = 7,
	['O'] = 7,
	['P'] = 7,
	['Q'] = 7,
	['R'] = 7,
	['S'] = 7,
	['T'] = 7,
	['U'] = 7,
	['V'] = 7,
	['W'] = 7,
	['X'] = 7,
	['Y'] = 7,
	['Z'] = 7,
	['['] = 4,
	['\\'] = 6,
	[']'] = 4,
	['^'] = 7,
	['_'] = 5,
	['`'] = 5,
	['{'] = 5,
	['|'] = 3,
	['}'] = 5,
	['~'] = 7,
	[127] = 7,
};

struct glyph {
	float x1, x2;
	int width;
};

struct font {
	struct texture *tex;
	int nglyphs, height;
	struct glyph glyphs[128];
};

struct font *font_load(const char *image, const char widths[128])
{
	int i;
	float inc, x = 0.0f;
	struct font *font = calloc(1, sizeof(*font));
	if (!font) {
		return NULL;
	}

	font->tex = texture_load("font.png");
	if (!font->tex) {
		goto fail;
	}
	texture_set_filter(font->tex, GL_NEAREST);

	font->height = font->tex->height;

	inc = 1.0f / (float)(font->tex->width);
	for (i = 0; i < 128; i++) {
		if (!widths[i]) {
			continue;
		}

		font->glyphs[i].x1 = x;
		x += widths[i] * inc;
		font->glyphs[i].x2 = x;
		font->glyphs[i].width = widths[i];
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
		vp[3] = 1.0f;

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
		vp[15] = 1.0f;

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

	mesh_dump(vertices, strlen(str) * 4, 1);
	mesh_indices_dump(indices, strlen(str) * 6);

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

	drawcall_set_mvp(dc, mvp);
	drawcall_set_texture(dc, "tex", GL_TEXTURE_2D, font->tex->handle);
	drawcall_set_viewport(dc, 0, 0, width, height);

	drawcall_set_attribute(dc, "position", 2, sizeof(float) * 4, (GLvoid *)0);
	drawcall_set_attribute(dc, "tc", 2, sizeof(float) * 4, (GLvoid *)(sizeof(float) * 2));

	dc->draw = draw_triangles;

	return dc;
}
