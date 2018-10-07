#version 100
varying highp vec2 v_TexCoord;
uniform sampler2D ytex;
uniform sampler2D utex;
uniform sampler2D vtex;

void main()
{
	// yuv2rgb conversion from
	// http://robotblogging.blogspot.co.uk/2013/10/gpu-accelerated-camera-processing-on.html
	highp float y = texture2D(ytex,v_TexCoord).r;
	highp float u = texture2D(utex,v_TexCoord).r;
	highp float v = texture2D(vtex,v_TexCoord).r;

	highp vec4 res;
	//res.r = (y + (1.370705 * (v-0.5)));
	//res.b = (y - (0.698001 * (v-0.5)) - (0.337633 * (u-0.5)));
	//res.g = (y + (1.732446 * (u-0.5)));
	res.r = (y + (1.370705 * (v-0.5)));
	res.b = (y - (0.698001 * (v-0.5)) - (0.337633 * (u-0.5)));
	res.g = (y + (1.732446 * (u-0.5)));
	res.a = 1.0;

	gl_FragColor = clamp(res,vec4(0),vec4(1));
}
