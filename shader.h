/*
 * Copyright Brian Starkey <stark3y@gmail.com> 2017
 *
 * With thanks to Ciro Santilli for his example here:
 * https://github.com/cirosantilli/cpp-cheat/blob/master/opengl/gles/triangle.c
 */
#ifndef __SHADER_H__
#define __SHADER_H__

char *shader_load(const char *filename);

GLint shader_compile(const char *vertex_shader_source, const char *fragment_shader_source);
GLint shader_load_compile_link(const char *vs_fname, const char *fs_fname);

#endif /* __SHADER_H__ */
