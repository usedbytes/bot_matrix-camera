/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 *
 * With thanks to Ciro Santilli for his example here:
 * https://github.com/cirosantilli/cpp-cheat/blob/master/opengl/gles/triangle.c
 */
#include <stdint.h>
#include <stdlib.h>

#define GLFW_INCLUDE_ES2
#include <GLFW/glfw3.h>

#include "pint.h"

#define GLFW_PINT(_pint) ((struct glfw_pint *)_pint)
struct glfw_pint {
	struct pint base;

	GLFWwindow* window;
};

static void swap_buffers(struct pint *p)
{
	struct glfw_pint *pint = GLFW_PINT(p);

	glfwSwapBuffers(pint->window);
}

static bool should_end(struct pint *p)
{
	struct glfw_pint *pint = GLFW_PINT(p);

	glfwPollEvents();
	return glfwWindowShouldClose(pint->window);
}

static void terminate(struct pint *p)
{
	p = NULL;
	glfwTerminate();
}

struct pint *pint_initialise(uint32_t width, uint32_t height)
{
	struct glfw_pint *pint = malloc(sizeof(*pint));
	if (!pint)
		return NULL;

	pint->base.swap_buffers = swap_buffers;
	pint->base.terminate = terminate;
	pint->base.should_end = should_end;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	pint->window = glfwCreateWindow(width, height, "llama", NULL, NULL);
	glfwMakeContextCurrent(pint->window);

	return (struct pint *)pint;
}
