#version 100
#extension GL_OES_EGL_image_external : require
uniform samplerExternalOES ytex;
uniform samplerExternalOES utex;
uniform samplerExternalOES vtex;
varying highp vec2 v_TexCoord;
void main()
{
	// yuv2rgb conversion from
	// http://robotblogging.blogspot.co.uk/2013/10/gpu-accelerated-camera-processing-on.html
	highp float y = texture2D(ytex,v_TexCoord).r;
	highp float u = texture2D(utex,v_TexCoord).r;
	highp float v = texture2D(vtex,v_TexCoord).r;

	highp vec4 rgb;
	rgb.r = (y + (1.370705 * (v-0.5)));
	rgb.g = (y - (0.698001 * (v-0.5)) - (0.337633 * (u-0.5)));
	rgb.b = (y + (1.732446 * (u-0.5)));
	rgb.a = 1.0;

	// gl_FragColor = clamp(res,vec4(0),vec4(1));

	// Crude white balance adjustment - with "SUNLIGHT" AWBMODE, and the
	// LEDs on the bot, this seems to give good results.
	gl_FragColor = clamp(vec4(rgb.r * 1.2, rgb.b * 0.75, rgb.g, 1.0), vec4(0), vec4(1));
}
