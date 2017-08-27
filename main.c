/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 *
 * With thanks to Ciro Santilli for his example here:
 * https://github.com/cirosantilli/cpp-cheat/blob/master/opengl/gles/triangle.c
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pam.h>

#include <GLES2/gl2.h>
#include "pint.h"
#include "shader.h"
#include "texture.h"
#include "mesh.h"

#define check(_cond) { if (!(_cond)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, strerror(errno)); exit(EXIT_FAILURE); }}

#define WIDTH 800
#define HEIGHT 600
#define MESHPOINTS 30

/*
 * Simple MVP matrix which flips the Y axis (so 0,0 is top left) and
 * scales/translates everything so that on-screen points are 0-1
 */
static const GLfloat mat[] = {
	2.0f,  0.0f,  0.0f, -1.0f,
	0.0f, -2.0f,  0.0f,  1.0f,
	0.0f,  0.0f,  0.0f,  0.0f,
	0.0f,  0.0f,  0.0f,  1.0f,
};

char **gargv;

void brown(float xcoord, float ycoord, float *xout, float *yout)
{
	double asp = (double)(WIDTH) / (double)(HEIGHT);
	double xoffs = ((double)(WIDTH - HEIGHT) / 2.0f) / (double)HEIGHT;
	float K[] = { 5.12f, -0.36f, 0.00f, 1.0f };
	//double xdiff = (xcoord * 2) - 1;
	double xdiff = (((xcoord * asp) - xoffs) * 2) - 1;
	double ydiff = (ycoord * 2) - 1;
	double r = sqrt(xdiff*xdiff + ydiff*ydiff);
	double newr;
	double xunit;
	double yunit;

	xunit = xdiff / r;
	if (isnan(xunit)) {
		xunit = 0;
	}

	yunit = ydiff / r;
	if (isnan(yunit)) {
		yunit = 0;
	}

	sscanf(gargv[1], "%f", &K[0]);
	sscanf(gargv[2], "%f", &K[1]);
	sscanf(gargv[3], "%f", &K[2]);
	sscanf(gargv[4], "%f", &K[3]);
	K[3] = K[3] - (K[0] + K[1] + K[2]);

	// Same algorithm used by ImageMagick.
	// Defined by Professor Helmut Dersch:
	// http://replay.waybackmachine.org/20090613040829/http://www.all-in-one.ee/~dersch/barrel/barrel.html
	// http://www.imagemagick.org/Usage/distorts/#barrel
	newr = r * (K[0]*pow(r, 3) + K[1]*pow(r,2) + K[2]*r + K[3]);

	*xout = (((newr*xunit + 1) / 2) + xoffs) / asp;
	*yout = (newr*yunit + 1) / 2;

	/*
	*xout = (0.5 + (xdiff / (1 + K[0]*(r*r) + K[1]*(r*r*r*r))));
	*yout = (0.5 + (ydiff / (1 + K[0]*(r*r) + K[1]*(r*r*r*r))));
	*/

	/*
	*xout = xcoord + (xdiff * K[0] * r * r) + (xdiff * K[1] * r * r *r * r);
	*yout = ycoord + (ydiff * K[0] * r * r) + (ydiff * K[1] * r * r *r * r);
	*/
}

GLint get_shader(void)
{
	char *vertex_shader_source, *fragment_shader_source;

	vertex_shader_source = shader_load("vertex_shader.glsl");
	if (!vertex_shader_source) {
		return -1;
	}
	printf("Vertex shader:\n");
	printf("%s\n", vertex_shader_source);

	fragment_shader_source = shader_load("fragment_shader.glsl");
	if (!fragment_shader_source) {
		return -1;
	}
	printf("Fragment shader:\n");
	printf("%s\n", fragment_shader_source);

	return shader_compile(vertex_shader_source, fragment_shader_source);
}

struct texture *get_texture(void)
{
	struct texture *tex = texture_load("texture.pnm");
	if (!tex) {
		fprintf(stderr, "Failed to get texture\n");
		return NULL;
	}

	glGenTextures(1, &tex->handle);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex->handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->width, tex->height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex->data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;
}

struct mesh {
	GLfloat *mesh;
	unsigned int nverts;
	GLuint mhandle;

	GLshort *indices;
	unsigned int nindices;
	GLuint ihandle;
};

struct mesh *get_mesh()
{
	struct mesh *mesh = calloc(1, sizeof(*mesh));
	if (!mesh) {
		return NULL;
	}

	mesh->mesh = mesh_build(MESHPOINTS, MESHPOINTS, brown, &mesh->nverts);
	if (!mesh->mesh) {
		free(mesh);
		return NULL;
	}

	glGenBuffers(1, &mesh->mhandle);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->mhandle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(mesh->mesh[0]) * mesh->nverts, mesh->mesh, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	mesh->indices = mesh_build_indices(MESHPOINTS, MESHPOINTS, &mesh->nindices);
	if (!mesh->indices) {
		free(mesh->mesh);
		free(mesh);
		return NULL;
	}

	glGenBuffers(1, &mesh->ihandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ihandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(mesh->indices[0]) * mesh->nindices, mesh->indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return mesh;
}

long elapsed_nanos(struct timespec a, struct timespec b)
{
	unsigned int sec = b.tv_sec - a.tv_sec;
	long nanos = b.tv_nsec - a.tv_nsec;

	return nanos + (1000000000 * sec);
}

int main(int argc, char *argv[]) {
	GLint ret;
	GLuint shader_program;
	GLint posLoc, tcLoc, mvpLoc, texLoc;
	struct texture *tex;
	struct mesh *mesh;
	struct timespec a, b;

	struct pint *pint = pint_initialise(WIDTH, HEIGHT);
	check(pint);

	gargv = argv;

	pm_init(argv[0], 0);

	printf("GL_VERSION  : %s\n", glGetString(GL_VERSION) );
	printf("GL_RENDERER : %s\n", glGetString(GL_RENDERER) );

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glViewport(0, 0, WIDTH, HEIGHT);

	ret = get_shader();
	check(ret >= 0);
	shader_program = ret;

	posLoc = glGetAttribLocation(shader_program, "position");
	tcLoc = glGetAttribLocation(shader_program, "tc");
	mvpLoc = glGetUniformLocation(shader_program, "mvp");
	texLoc = glGetUniformLocation(shader_program, "tex");

	tex = get_texture();
	check(tex);

	mesh = get_mesh();
	check(mesh);
	printf("Mesh:\n");
	mesh_dump(mesh->mesh, MESHPOINTS, MESHPOINTS);
	printf("Indices:\n");
	mesh_indices_dump(mesh->indices, mesh->nindices);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->mhandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ihandle);
	glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(mesh->mesh[0]) * 4, (GLvoid*)0);
	glVertexAttribPointer(tcLoc, 2, GL_FLOAT, GL_FALSE, sizeof(mesh->mesh[0]) * 4, (GLvoid*)(sizeof(mesh->mesh[0]) * 2));
	glEnableVertexAttribArray(posLoc);
	glEnableVertexAttribArray(tcLoc);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	clock_gettime(CLOCK_MONOTONIC, &a);
	while(!pint->should_end(pint)) {
		glClear(GL_COLOR_BUFFER_BIT);

#if 1
		glUseProgram(shader_program);
		glUniform1i(texLoc, 0);
		glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mat);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->handle);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->mhandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ihandle);
		glDrawElements(GL_TRIANGLE_STRIP, mesh->nindices, GL_UNSIGNED_SHORT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#else
		glUseProgram(shader_program);
		glUniform1i(texLoc, 0);
		glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mat);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->handle);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->mhandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ihandle);
		glDrawElements(GL_LINE_STRIP, mesh->nindices, GL_UNSIGNED_SHORT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif

		pint->swap_buffers(pint);
		clock_gettime(CLOCK_MONOTONIC, &b);
		if (a.tv_sec != b.tv_sec) {
			long time = elapsed_nanos(a, b);
			printf("%.3f fps\n", 1000000000.0 / (float)time);
		}
		a = b;
	}

	pint->terminate(pint);

	return EXIT_SUCCESS;
}
