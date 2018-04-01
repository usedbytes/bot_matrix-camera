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

struct texture *texture_load(const char *file)
{
	struct texture *tex;
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

	ret = load_pnm(tex, fp);
	if (ret) {
		free(tex);
		tex = NULL;
	}

	fclose(fp);
	return tex;
}
