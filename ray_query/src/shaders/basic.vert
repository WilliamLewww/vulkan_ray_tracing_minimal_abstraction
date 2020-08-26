#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

layout(binding = 1) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
} camera;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragColor;

void main() {
	mat4 viewMatrix = {
		vec4(camera.right.x, camera.up.x, camera.forward.x, 0),
		vec4(camera.right.y, camera.up.y, camera.forward.y, 0),
		vec4(camera.right.z, camera.up.z, camera.forward.z, 0),
		vec4(-dot(camera.right, camera.position), -dot(camera.up, camera.position), -dot(camera.forward, camera.position), 1)
	};

  gl_Position = vec4(inPosition, 1.0);
  fragColor = vec3(1, 1, 1);
}
