/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh.h"

double *mesh_load(const char *file, int *width, int *height) {
	int i, j, ret;
	double *mesh;
	char *line = NULL;
	size_t n = 0;
	FILE *fp = fopen(file, "r");
	if (!fp) {
		fprintf(stderr, "Couldn't open file.\n");
		return NULL;
	}

	ret = fscanf(fp, "%d,%d\n", width, height);
	if (ret != 2) {
		fprintf(stderr, "Couldn't get w/h\n");
		return NULL;
	}

	if (*width != *height) {
		fprintf(stderr, "Only square meshes are supported.\n");
		return NULL;
	}

	mesh = calloc((*width) * (*height) * 2, sizeof(*mesh));
	if (!mesh) {
		return NULL;
	}

	for (i = 0; i < (*height); i++) {
		char *tok;

		ret = getline(&line, &n, fp);
		if (ret < 0) {
			fprintf(stderr, "Failed reading line %d\n", i);
			return NULL;
		}
		tok = strtok(line, ",");
		for (j = 0; j < (*width) * 2; j++) {
			ret = sscanf(tok, "%lf", &mesh[i * (*width) * 2 + j]);
			if (ret != 1) {
				fprintf(stderr, "Failed reading item %d,%d\n", j,i);
				return NULL;
			}

			tok = strtok(NULL, ",");
			if (!tok && j < ((*width) * 2) - 1) {
				fprintf(stderr, "Line %d ended too soon\n", i);
				return NULL;
			}
		}
		free(line);
		n = 0;
		line = NULL;
	}

	return mesh;
}

GLfloat *mesh_build_from_file(const char *file, unsigned int *nelems, int *xp, int *yp) {
	int xpoints, ypoints;
	double *arr = mesh_load(file, &xpoints, &ypoints);
	if (!arr) {
		return NULL;
	}

	unsigned int row, col, nmesh = xpoints * ypoints * 4;
	double xstep = (double)1.0f / (xpoints - 1);
	double ystep = (double)1.0f / (ypoints - 1);
	unsigned int i = 0;

	GLfloat *cursor;
	GLfloat *mesh = malloc(sizeof(*mesh) * nmesh);
	if (!mesh) {
		return NULL;
	}

	cursor = mesh;
	for (row = 0; row < ypoints; row++) {
		GLfloat y = row * ystep;
		for (col = 0; col < xpoints; col++, cursor += 4, i++) {
			GLfloat x = col * xstep;

			cursor[0] = x;
			cursor[1] = y;

			cursor[2] = arr[(row * xpoints * 2) + (col * 2)];
			cursor[3] = arr[(row * xpoints * 2) + (col * 2) + 1];
		}
	}

	if (nelems) {
		*nelems = nmesh;
	}

	if (xp) {
		*xp = xpoints;
	}

	if (yp) {
		*yp = ypoints;
	}

	return mesh;
}

GLfloat *mesh_build(unsigned int xpoints, unsigned int ypoints, tex_coord_func texfunc,
		    unsigned int *nelems)
{
	unsigned int row, col, nmesh = xpoints * ypoints * 4;
	double xstep = (double)1.0f / (xpoints - 1);
	double ystep = (double)1.0f / (ypoints - 1);
	unsigned int i = 0;

	GLfloat *cursor;
	GLfloat *mesh = malloc(sizeof(*mesh) * nmesh);
	if (!mesh) {
		return NULL;
	}

	cursor = mesh;
	for (row = 0; row < ypoints; row++) {
		GLfloat y = row * ystep;
		for (col = 0; col < xpoints; col++, cursor += 4, i++) {
			GLfloat x = col * xstep;

			cursor[0] = x;
			cursor[1] = y;

			if (texfunc) {
				texfunc(x, y, &cursor[2], &cursor[3]);
			} else {
				cursor[2] = x;
				cursor[3] = y;
			}
		}
	}

	if (nelems) {
		*nelems = nmesh;
	}

	return mesh;
}

GLshort *mesh_build_indices(unsigned int xpoints, unsigned int ypoints,  unsigned int *nindices)
{
	unsigned int nrows = ypoints - 1;
	unsigned int tris_per_row = (xpoints - 1) * 2;
	unsigned int idx_per_row = tris_per_row + 4;

	unsigned int nidx = (nrows * idx_per_row) - 2;
	unsigned int row, i, offset = 0;

	GLshort *indices = malloc(sizeof(*indices) * nidx);
	if (!indices) {
		return NULL;
	}

	for (row = 0; row < nrows; row++, offset += 2) {
		for (i = 0; i < nrows + 1; i++, offset += 2) {
			indices[offset] = row * xpoints + i;
			indices[offset + 1] = row * xpoints + i + xpoints;
		}
		if (row < nrows - 1) {
			indices[offset] = indices[offset - 1];
			indices[offset + 1] = (row + 1) * xpoints;
		}
	}

	if (nindices) {
		*nindices = nidx;
	}

	return indices;
}

void mesh_dump(GLfloat *mesh, unsigned int xpoints, unsigned int ypoints)
{
	unsigned int row, col;
	unsigned int offset = 0;

	for (row = 0; row < ypoints; row++) {
		for (col = 0; col < xpoints; col++, offset += 4) {
			printf("(%1.3f %1.3f)<-(%1.3f %1.3f) | ",
				mesh[offset + 0],
				mesh[offset + 1],
				mesh[offset + 2],
				mesh[offset + 3]);
		}
		printf("\n");
	}
}

void mesh_indices_dump(GLshort *indices, unsigned int nindices)
{
	unsigned int i;
	for (i = 0; i < nindices; i++) {
		printf("%u, ", indices[i]);
	}
	printf("\n");
}
