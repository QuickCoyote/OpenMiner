#version 120

attribute vec4 color;
attribute vec4 coord3d;
attribute vec3 normal;
attribute vec2 texCoord;

varying vec4 v_color;
varying vec4 v_coord3d;
varying vec4 v_normal;
varying vec2 v_texCoord;

uniform mat4 u_modelMatrix;
uniform mat4 u_projectionMatrix;
uniform mat4 u_viewMatrix;

void main() {
	v_coord3d = u_modelMatrix * vec4(coord3d.xyz, 1.0);
	v_normal = vec4(normal, 1.0);
	v_color = color;
	v_texCoord = texCoord;

	// Get rid of unused 'normal' attribute warning
	v_coord3d = v_coord3d + v_normal * 0.00000000000000000001;

	gl_Position = u_projectionMatrix * u_viewMatrix * v_coord3d;
}
