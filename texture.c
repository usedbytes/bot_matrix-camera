/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pam.h>

#include "texture.h"

#define ALIGN_UP(_size, _base) ((((_size) + ((_base) - 1))) & (~((_base) - 1)))
struct texture *texture_load(const char *file)
{
	struct texture *tex;
	struct pam inpam = { 0 };
	tuple * tuplerow;
	unsigned int row, col, offset = 0;
	unsigned int pitch;

	FILE *fp = fopen(file, "r");
	if (!fp) {
		fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
		return NULL;
	}

	pnm_readpaminit(fp, &inpam, PAM_STRUCT_SIZE(tuple_type));

	tuplerow = pnm_allocpamrow(&inpam);

	tex = calloc(1, sizeof(*tex));
	if (!tex) {
		fclose(fp);
		return NULL;
	}

	tex->width = inpam.width;
	tex->height = inpam.height;

	pitch = ALIGN_UP(inpam.width * inpam.depth, 4);
	tex->datalen = pitch * inpam.height;
	tex->data = calloc(tex->datalen, 1);
	if (!tex->data) {
		free(tex);
		fclose(fp);
		return NULL;
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

	fclose(fp);
	return tex;
}
