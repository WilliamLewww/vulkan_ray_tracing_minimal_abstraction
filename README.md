# vulkan_ray_tracing_minimal_abstraction

There are two different sub-projects found in this repository, "ray_pipeline" and "ray_query". Both projects use a simple global illumination model but they differ in the manner of casting the rays.

"ray_pipeline" uses the ray tracing pipeline with a shader binding table to cast the rays.

"ray_query" uses ray querying in the fragment shader to cast rays.