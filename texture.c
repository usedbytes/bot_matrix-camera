/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <GLES2/gl2.h>
#include <GLES/gl.h>

#include <pam.h>
#include <png.h>

#include "texture.h"

#define ALIGN_UP(_size, _base) ((((_size) + ((_base) - 1))) & (~((_base) - 1)))

//https://stackoverflow.com/questions/5309471/getting-file-extension-in-c
static char *get_filename_ext(const char *filename) {
	int i;
	char *dup;
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	dup = strdup(dot + 1);
	for (i = 0; dup[i]; i++) {
		 dup[i] = tolower(dup[i]);
	}

	return dup;
}

int load_pnm(struct texture *tex, FILE *fp)
{
	struct pam inpam = { 0 };
	tuple * tuplerow;
	unsigned int row, col, offset = 0;
	unsigned int pitch;

#ifndef PAM_STRUCT_SIZE
	pnm_readpaminit(fp, &inpam, sizeof(inpam));
#else
	pnm_readpaminit(fp, &inpam, PAM_STRUCT_SIZE(tuple_type));
#endif

	tuplerow = pnm_allocpamrow(&inpam);

	tex->width = inpam.width;
	tex->height = inpam.height;
	tex->ncmp = inpam.depth;

	pitch = ALIGN_UP(inpam.width * inpam.depth, 4);
	tex->datalen = pitch * inpam.height;
	tex->data = calloc(tex->datalen, 1);
	if (!tex->data) {
		return -1;
	}

	for (row = 0; row < tex->height; row++, offset += pitch) {
		char *pixel = tex->data + offset;
		pnm_readpamrow(&inpam, tuplerow);

		for (col = 0; col < inpam.width; col++, pixel += inpam.depth) {
			unsigned int plane;

			for (plane = 0; plane < inpam.depth; ++plane) {
				pixel[plane] = tuplerow[col][plane];
			}
		}
	}
	pnm_freepamrow(tuplerow);

	return 0;
}

int load_png(struct texture *tex, FILE *fp)
{
	/*
	 * A simple libpng example program
	 * http://zarb.org/~gc/html/libpng.html
	 *
	 * Modified by Yoshimasa Niwa to make it much simpler
	 * and support all defined color_type.
	 *
	 * To build, use the next instruction on OS X.
	 * $ brew install libpng
	 * $ clang -lz -lpng15 libpng_test.c
	 *
	 * Copyright 2002-2010 Guillaume Cottenceau.
	 *
	 * This software may be freely redistributed under the terms
	 * of the X11 license.
	 *
	 */
	png_byte color_type;
	png_byte bit_depth;
	png_bytep *row_pointers;


	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png) {
		fprintf(stderr, "Couldn't create png read struct\n");
		return -1;
	}

	png_infop info = png_create_info_struct(png);
	if(!info) {
		fprintf(stderr, "Couldn't create info struct\n");
		return -1;
	}

	png_init_io(png, fp);

	png_read_info(png, info);

	tex->width = png_get_image_width(png, info);
	tex->height = png_get_image_height(png, info);
	tex->ncmp = 4;
	color_type = png_get_color_type(png, info);
	bit_depth = png_get_bit_depth(png, info);

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if(bit_depth == 16)
		png_set_strip_16(png);

	if(color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);

	if(png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if(color_type == PNG_COLOR_TYPE_RGB ||
			color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	if(color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	png_read_update_info(png, info);

	tex->datalen = ALIGN_UP(png_get_rowbytes(png, info), 4) * tex->height;
	tex->data = calloc(tex->datalen, 1);
	if (!tex->data) {
		return -1;
	}

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * tex->height);
	for(int y = 0; y < tex->height; y++) {
		row_pointers[y] = (png_bytep)&tex->data[ALIGN_UP(png_get_rowbytes(png, info), 4) * y];
	}

	png_read_image(png, row_pointers);

	free(row_pointers);

	return 0;
}

struct texture *texture_load(const char *file)
{
	GLint format;
	struct texture *tex;
	char *ext;
	int ret;

	FILE *fp = fopen(file, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
		return NULL;
	}

	tex = calloc(1, sizeof(*tex));
	if (!tex) {
		fclose(fp);
		return NULL;
	}

	ext = get_filename_ext(file);
	if (!strcmp(ext, "pnm") || !strcmp(ext, "pbm") || !strcmp(ext, "pgm")) {
		ret = load_pnm(tex, fp);
	} else if (!strcmp(ext, "png")) {
		ret = load_png(tex, fp);
	} else {
		fprintf(stderr, "Couldn't load image type %s\n", ext);
		ret = -1;
	}
	free(ext);

	if (ret) {
		free(tex);
		tex = NULL;
	}

	fclose(fp);

	glGenTextures(1, &tex->handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	switch (tex->ncmp) {
		case 4:
			format = GL_RGBA;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 1:
			format = GL_LUMINANCE;
			break;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, format, tex->width, tex->height, 0, format, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;
}

void texture_set_filter(struct texture *tex, GLint mode)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
	glBindTexture(GL_TEXTURE_2D, 0);
}
