#version 100
uniform highp mat4 mvp;
attribute vec2 position;
attribute vec2 tc;
varying highp vec2 v_TexCoord;

void main() {
	gl_Position = vec4(position, 0, 1) * mvp;
	v_TexCoord = tc;
}
