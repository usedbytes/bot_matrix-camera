/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 */
#ifndef __MESH_H__
#define __MESH_H__

#include <GLES2/gl2.h>

typedef void (*tex_coord_func)(float inx, float iny, float *outx, float *outy);

GLfloat *mesh_build(unsigned int xpoints, unsigned int ypoints, tex_coord_func texfunc,
		    unsigned int *nelems);
GLshort *mesh_build_indices(unsigned int xpoints, unsigned int ypoints,  unsigned int *nindices);

void mesh_dump(GLfloat *mesh, unsigned int xpoints, unsigned int ypoints);
void mesh_indices_dump(GLshort *indices, unsigned int nindices);
GLfloat *mesh_build_from_file(const char *file, unsigned int *nelems, int *xpoints, int *ypoints);

#endif /* __MESH_H__ */
