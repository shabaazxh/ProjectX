#version 450

//layout(location = 0) out vec4 fragColor;

void main()
{
	//fragColor = vec4(0.0, 0.0, 0.0, 1.0);	
	gl_FragDepth = gl_FragCoord.z;
}
