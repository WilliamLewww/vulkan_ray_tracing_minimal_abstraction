# vulkan_ray_tracing_minimal_abstraction

There are two different sub-projects found in this repository, "ray_pipeline" and "ray_query". Both projects use a simple global illumination model but they differ in the manner of casting the rays.

"ray_pipeline" uses the ray tracing pipeline with a shader binding table to cast the rays.

"ray_query" uses ray querying in the fragment shader to cast rays.


## Important

Make sure the *VK_KHR_ray_tracing* extension is available on your device! You may need to install the Nvidia Vulkan Beta drivers to use the extension; *Even if you have a compatible RTX graphics card, your drivers may not have the extension available.*

![beta-drivers](resources/drivers.png)

To see if the extension is available, you can use the *vulkaninfo* program.

![vulkaninfo](resources/vulkaninfo.png)