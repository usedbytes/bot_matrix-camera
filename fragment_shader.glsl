#version 100
varying highp vec2 v_TexCoord;
uniform sampler2D tex;

void main()
{
	gl_FragColor = texture2D(tex, v_TexCoord);
}
