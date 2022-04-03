#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#define M_PI 3.1415926535897932384626433832795

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
}
camera;

layout(binding = 2, set = 0) buffer IndexBuffer { uint data[]; }
indexBuffer;
layout(binding = 3, set = 0) buffer VertexBuffer { float data[]; }
vertexBuffer;
layout(binding = 4, set = 0, rgba32f) uniform image2D image;

layout(binding = 0, set = 1) buffer MaterialIndexBuffer { uint data[]; }
materialIndexBuffer;

struct Material {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 emission;
};

layout(binding = 1, set = 1) buffer MaterialBuffer { Material data[]; }
materialBuffer;

void main() {
  outColor = vec4(1.0);
}
