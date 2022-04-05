# Vulkan Ray Tracing Minimal Abstraction
#### Currently undergoing a revamp (less abstraction + cleaner structure initialization) in the wlew/revamp branch.

<p float="left">
  <img src="resources/render_1.png" width="400">
  <img src="resources/render_2.png" width="400">
</p>

There are two different sub-projects found in this repository. Both projects use a simple global illumination model but they differ in the manner of casting the rays.

**ray_pipeline** uses the ray tracing pipeline with a shader binding table to cast the rays.

**ray_query** uses ray querying in the fragment shader to cast rays.


## Important (Current 455.46.04)

Make sure the **VK_KHR_ray_query** and  **VK_KHR_ray_tracing_pipeline** extensions are available on your device! You may need to install the Nvidia Vulkan Beta drivers to use the extension.

**Even if you have a compatible RTX graphics card, your drivers may not have the extension available.**

### [**Downloads Beta Drivers**](https://developer.nvidia.com/vulkan-driver)

![beta-drivers](resources/drivers.png)

To see if the extension is available, you can use the **vulkaninfo** program.

![vulkaninfo](resources/vulkaninfo.png)
