#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
} camera;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 color;

vec3 positions[3] = vec3[](
    vec3(0.0, -0.5, 0.0),
    vec3(0.5, 0.5, 0.0),
    vec3(-0.5, 0.5, 0.0)
);

void main() {
	mat4 viewMatrix = {
		vec4(camera.right.x, camera.up.x, camera.forward.x, 0),
		vec4(camera.right.y, camera.up.y, camera.forward.y, 0),
		vec4(camera.right.z, camera.up.z, camera.forward.z, 0),
		vec4(-dot(camera.right, camera.position), -dot(camera.up, camera.position), -dot(camera.forward, camera.position), 1)
	};

	float farDist = 1000.0;
	float nearDist = 0.0001;
	float frustumDepth = farDist - nearDist;
	float oneOverDepth = 1.0 / frustumDepth;
	float fov = 1.0472;
	float aspect = 800.0 / 600.0;

	mat4 projectionMatrix = {
		vec4(1.0 / tan(0.5f * fov) / aspect, 0, 0, 0),
		vec4(0, 1.0 / tan(0.5f * fov), 0, 0),
		vec4(0, 0, farDist * oneOverDepth, 1),
		vec4(0, 0, (-farDist * nearDist) * oneOverDepth, 0)
	};

  gl_Position = projectionMatrix * viewMatrix * vec4(inPosition, 1.0);
  color = inPosition;
}
