#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#define M_PI 3.1415926535897932384626433832795

struct Material {
	vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 emission;
};

hitAttributeEXT vec2 hitCoordinate;

layout(location = 0) rayPayloadInEXT Payload {
  vec3 rayOrigin;
  vec3 rayDirection;
  vec3 accumulatedColor;
  int rayDepth;

  vec2 pixel;
} payload;

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
} camera;

layout(binding = 3, set = 0) buffer IndexBuffer { uint data[]; } indexBuffer;
layout(binding = 4, set = 0) buffer VertexBuffer { float data[]; } vertexBuffer;

layout(binding = 0, set = 1) buffer MaterialIndexBuffer { uint data[]; } materialIndexBuffer;
layout(binding = 1, set = 1) buffer MaterialBuffer { Material data[]; } materialBuffer;

float random(float u, float v) {
  return fract(sin(dot(vec2(u, v), vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 sampleCosineWeightedHemisphere(float u, float v) {
	float phi = 2.0f * M_PI * u;

	float cosPhi = cos(phi);
	float sinPhi = sin(phi);

	float cosTheta = sqrt(v);
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	return vec3(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
}

vec3 alignHemisphereWithNormal(vec3 hemisphere, vec3 normal) {
	vec3 right = normalize(cross(normal, vec3(0.0072f, 1.0f, 0.0034f)));
	vec3 forward = cross(right, normal);

	return hemisphere.x * right + hemisphere.y * normal + hemisphere.z * forward;
}

void main() {
	ivec3 indices = ivec3(indexBuffer.data[3 * gl_PrimitiveID + 0], indexBuffer.data[3 * gl_PrimitiveID + 1], indexBuffer.data[3 * gl_PrimitiveID + 2]);

	vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

	vec3 vertexA = vec3(vertexBuffer.data[3 * indices.x + 0], vertexBuffer.data[3 * indices.x + 1], vertexBuffer.data[3 * indices.x + 2]);
	vec3 vertexB = vec3(vertexBuffer.data[3 * indices.y + 0], vertexBuffer.data[3 * indices.y + 1], vertexBuffer.data[3 * indices.y + 2]);
	vec3 vertexC = vec3(vertexBuffer.data[3 * indices.z + 0], vertexBuffer.data[3 * indices.z + 1], vertexBuffer.data[3 * indices.z + 2]);

	vec3 position = vertexA * barycentric.x + vertexB * barycentric.y + vertexC * barycentric.z;
	vec3 geometricNormal = normalize(cross(vertexB - vertexA, vertexC - vertexA));

	vec3 surfaceColor = materialBuffer.data[materialIndexBuffer.data[gl_PrimitiveID]].diffuse;

	{
		// 34 & 35 == light
		vec3 lightColor = vec3(0.6, 0.6, 0.6);

		ivec3 lightIndices = ivec3(indexBuffer.data[3 * 34 + 0], indexBuffer.data[3 * 34 + 1], indexBuffer.data[3 * 34 + 2]);

		vec3 lightVertexA = vec3(vertexBuffer.data[3 * lightIndices.x + 0], vertexBuffer.data[3 * lightIndices.x + 1], vertexBuffer.data[3 * lightIndices.x + 2]);
		vec3 lightVertexB = vec3(vertexBuffer.data[3 * lightIndices.y + 0], vertexBuffer.data[3 * lightIndices.y + 1], vertexBuffer.data[3 * lightIndices.y + 2]);
		vec3 lightVertexC = vec3(vertexBuffer.data[3 * lightIndices.z + 0], vertexBuffer.data[3 * lightIndices.z + 1], vertexBuffer.data[3 * lightIndices.z + 2]);

		vec2 uv = vec2(random(camera.frameCount, hitCoordinate.x), random(camera.frameCount, hitCoordinate.y));
		if (uv.x + uv.y > 1.0f) {
			uv.x = 1.0f - uv.x;
			uv.y = 1.0f - uv.y;
		}

		vec3 lightBarycentric = vec3(1.0 - uv.x - uv.y, uv.x, uv.y);
		vec3 lightPosition = lightVertexA * lightBarycentric.x + lightVertexB * lightBarycentric.y + lightVertexC * lightBarycentric.z;

		vec3 positionToLightDirection = normalize(lightPosition - position);

		vec3 shadowRayOrigin = position;
		vec3 shadowRayDirection = positionToLightDirection;
		float shadowRayDistance = length(lightPosition - position) - 0.001f;
		uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;

		isShadow = true;
		traceRayEXT(topLevelAS, shadowRayFlags, 0xFF, 0, 0, 1, shadowRayOrigin, 0.001, shadowRayDirection, shadowRayDistance, 1);

		if (isShadow) {
			if (payload.rayDepth == 0) {
				payload.accumulatedColor = vec3(0.0, 0.0, 0.0);
			}
		}
		else {
			if (payload.rayDepth == 0) {
				payload.accumulatedColor *= vec3(surfaceColor * lightColor * dot(geometricNormal, positionToLightDirection));
			}
			else {
				payload.accumulatedColor += (1.0 / payload.rayDepth) * vec3(surfaceColor * lightColor * dot(geometricNormal, positionToLightDirection));
			}
		}

		payload.rayDepth += 1;
	}

	vec3 sampleDirection = sampleCosineWeightedHemisphere(random(camera.frameCount, hitCoordinate.x), random(camera.frameCount, hitCoordinate.y));
	sampleDirection = alignHemisphereWithNormal(sampleDirection, geometricNormal);

	payload.rayOrigin = position + geometricNormal * 0.001f;
	payload.rayDirection = sampleDirection;
}
