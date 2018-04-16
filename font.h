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
void font_calculate(struct font *font, struct drawcall *dc, const char *str, float size);
struct drawcall *create_font_drawcall(struct font *font, int width, int height);

#endif /* __FONT_H__ */
