#version 410

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec2 outUV;
layout(location = 4) out float outHeight;

uniform mat4 u_ModelViewProjection;
uniform mat4 u_View;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform vec3 u_LightPos;

uniform sampler2D s_Height;

void main() {

	// Lecture 5
	// Pass vertex pos in world space to frag shader
	vec3 v = inPosition;
	v.z = texture(s_Height, inUV).r;
	
	outHeight = v.z;

	//outPos = (u_Model * vec4(v, 1.0)).xyz;
	outPos = v;

	// Normals
	outNormal = u_NormalMatrix * inNormal;

	// Pass our UV coords to the fragment shader
	outUV = inUV;

	///////////
	outColor = inColor;

	gl_Position = u_ModelViewProjection * vec4(v, 1.0);

}

