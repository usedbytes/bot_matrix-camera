#version 100
#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES tex;
varying highp vec2 v_TexCoord;
void main()
{
	gl_FragColor = texture2D(tex, v_TexCoord);
}
