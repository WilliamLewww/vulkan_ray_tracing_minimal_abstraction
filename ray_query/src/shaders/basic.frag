#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
} camera;

void main() {
  vec3 origin = vec3(0, 0, -5);
  vec3 direction = vec3(0, 0, 1);

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, 0.001f, direction, 1000.0f);

  while (rayQueryProceedEXT(rayQuery));

  if (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
  }

  outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
