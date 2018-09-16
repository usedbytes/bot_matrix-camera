/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2018
 */
#ifndef __FONT_H__
#define __FONT_H__
#include <stdint.h>
#include <stdbool.h>

#include "drawcall.h"

struct font;

struct font *font_load(const char *image, const char *charset);
struct drawcall *create_font_drawcall(struct font *font, int width, int height);
float font_calculate_width(struct font *font, const char *str, float size);

struct element_array {
	size_t nverts;
	size_t nidx;
	float *vertices;
	GLshort *indices;
};

struct element_array *element_array_alloc(size_t nverts, size_t nidx);
void element_array_free(struct element_array *a);
void element_array_append(struct element_array *a, struct element_array *b);
struct element_array *font_calculate(struct font *font, const char *str,
			    float x, float y, float size);

#endif /* __FONT_H__ */
