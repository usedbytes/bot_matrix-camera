/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 *
 * With thanks to Ciro Santilli for his example here:
 * https://github.com/cirosantilli/cpp-cheat/blob/master/opengl/gles/triangle.c
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <GLES2/gl2.h>

char *shader_load(const char *filename)
{
	int ret;
	long len;
	char *shader;

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Couldn't open %s: %s\n", filename, strerror(errno));
		return NULL;
	}

	ret = fseek(fp, 0, SEEK_END);
	if (ret != 0) {
		fprintf(stderr, "Couldn't seek: %s\n", strerror(errno));
		fclose(fp);
		return NULL;
	}

	len = ftell(fp);
	if (len < 0) {
		fprintf(stderr, "Couldn't ftell: %s\n", strerror(errno));
		fclose(fp);
		return NULL;

	}

	rewind(fp);

	shader = calloc(1, len);
	if (!shader) {
		fprintf(stderr, "Couldn't alloc shader\n");
		fclose(fp);
		return NULL;
	}

	if (fread(shader, len, 1, fp) != 1) {
		fprintf(stderr, "Couldn't read shader: %s\n", strerror(errno));
		free(shader);
		fclose(fp);
		return NULL;
	}
	shader[len-1] = '\0';

	fclose(fp);
	return shader;
}

GLint shader_compile(const char *vertex_shader_source, const char *fragment_shader_source) {
	enum Consts {INFOLOG_LEN = 512};
	GLchar infoLog[INFOLOG_LEN];
	GLint fragment_shader;
	GLint shader_program;
	GLint success;
	GLint vertex_shader;

	/* Vertex shader */
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, INFOLOG_LEN, NULL, infoLog);
		fprintf(stderr, "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s\n", infoLog);
		return -1;
	}

	/* Fragment shader */
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_shader, INFOLOG_LEN, NULL, infoLog);
		fprintf(stderr, "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s\n", infoLog);
		return -1;
	}

	/* Link shaders */
	shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader_program, INFOLOG_LEN, NULL, infoLog);
		fprintf(stderr, "ERROR::SHADER::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
		return -1;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return shader_program;
}
