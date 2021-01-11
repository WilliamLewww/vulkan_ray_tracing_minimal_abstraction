#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MAX_FRAMES_IN_FLIGHT      1
#define ENABLE_VALIDATION         1

static char keyDownIndex[500];

static float cameraPosition[3];
static float cameraYaw;
static float cameraPitch;

struct Camera {
  float position[4];
  float right[4];
  float up[4];
  float forward[4];

  uint32_t frameCount;
};

struct Material {
  float ambient[3]; int padA;
  float diffuse[3]; int padB;
  float specular[3]; int padC;
  float emission[3]; int padD;
};

struct Scene {
  tinyobj_attrib_t attributes;
  tinyobj_shape_t* shapes;
  tinyobj_material_t* materials;

  uint64_t numShapes;
  uint64_t numMaterials;
};

struct VulkanApplication {
  GLFWwindow* window;
  VkSurfaceKHR surface;
  VkInstance instance;

  VkDebugUtilsMessengerEXT debugMessenger;

  VkPhysicalDevice physicalDevice;
  VkDevice logicalDevice;

  uint32_t graphicsQueueIndex;
  uint32_t presentQueueIndex;
  uint32_t computeQueueIndex;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkQueue computeQueue;

  uint32_t imageCount;
  VkSwapchainKHR swapchain;
  VkImage* swapchainImages;
  VkFormat swapchainImageFormat;

  VkCommandPool commandPool;

  VkSemaphore* imageAvailableSemaphores;
  VkSemaphore* renderFinishedSemaphores;
  VkFence* inFlightFences;
  VkFence* imagesInFlight;
  uint32_t currentFrame;

  VkPhysicalDeviceMemoryProperties memoryProperties;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;

  VkBuffer vertexPositionBuffer;
  VkDeviceMemory vertexPositionBufferMemory;

  VkBuffer materialIndexBuffer;
  VkDeviceMemory materialIndexBufferMemory;

  VkBuffer materialBuffer;
  VkDeviceMemory materialBufferMemory;

  VkAccelerationStructureKHR bottomLevelAccelerationStructure;
  VkBuffer bottomLevelAccelerationStructureBuffer;
  VkDeviceMemory bottomLevelAccelerationStructureBufferMemory;

  VkAccelerationStructureKHR topLevelAccelerationStructure;
  VkBuffer topLevelAccelerationStructureBuffer;
  VkDeviceMemory topLevelAccelerationStructureBufferMemory;

  VkImageView rayTraceImageView;
  VkImage rayTraceImage;
  VkDeviceMemory rayTraceImageMemory;

  VkBuffer uniformBuffer;
  VkDeviceMemory uniformBufferMemory;

  VkDescriptorPool descriptorPool;
  VkDescriptorSet rayTraceDescriptorSet;
  VkDescriptorSet materialDescriptorSet;
  VkDescriptorSetLayout* rayTraceDescriptorSetLayouts;

  VkPipeline rayTracePipeline;
  VkPipelineLayout rayTracePipelineLayout;

  VkBuffer shaderBindingTableBuffer;
  VkDeviceMemory shaderBindingTableBufferMemory;

  VkCommandBuffer* commandBuffers;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  printf("\033[22;36mvalidation layer\033[0m: \033[22;33m%s\033[0m\n", pCallbackData->pMessage);  

  return VK_FALSE;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    keyDownIndex[key] = 1;
  }
  if (action == GLFW_RELEASE) {
    keyDownIndex[key] = 0;
  }
}

void readFile(const char* fileName, char** buffer, uint64_t* length) {
  uint64_t stringSize = 0;
  uint64_t readSize = 0;
  FILE * handler = fopen(fileName, "r");

  if (handler) {
    fseek(handler, 0, SEEK_END);
    stringSize = ftell(handler);
    rewind(handler);
    *buffer = (char*)malloc(sizeof(char) * (stringSize + 1));
    readSize = fread(*buffer, sizeof(char), (size_t)stringSize, handler);
    (*buffer)[stringSize] = '\0';
    if (stringSize != readSize) {
      free(buffer);
      *buffer = NULL;
    }
    fclose(handler);
  }

  *length = readSize;
}

void createBuffer(struct VulkanApplication* app, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = usageFlags;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(app->logicalDevice, &bufferCreateInfo, NULL, buffer) == VK_SUCCESS) {
    printf("created buffer with size %ld\n", size);
  }

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(app->logicalDevice, *buffer, &memoryRequirements);

  VkMemoryAllocateInfo memoryAllocateInfo = {};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;

  uint32_t memoryTypeIndex = -1;
  for (int x = 0; x < app->memoryProperties.memoryTypeCount; x++) {
    if ((memoryRequirements.memoryTypeBits & (1 << x)) && (app->memoryProperties.memoryTypes[x].propertyFlags & propertyFlags) == propertyFlags) {
      memoryTypeIndex = x;
      break;
    }
  }
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

  if (vkAllocateMemory(app->logicalDevice, &memoryAllocateInfo, NULL, bufferMemory) == VK_SUCCESS) {
    printf("allocated buffer memory\n");
  }

  vkBindBufferMemory(app->logicalDevice, *buffer, *bufferMemory, 0);
}

void copyBuffer(struct VulkanApplication* app, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBufferAllocateInfo bufferAllocateInfo = {};
  bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandPool = app->commandPool;
  bufferAllocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(app->logicalDevice, &bufferAllocateInfo, &commandBuffer);
  
  VkCommandBufferBeginInfo commandBufferBeginInfo = {};
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  
  vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  VkBufferCopy bufferCopy = {};
  bufferCopy.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopy);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(app->graphicsQueue);

  vkFreeCommandBuffers(app->logicalDevice, app->commandPool, 1, &commandBuffer);
}

void createImage(struct VulkanApplication* app, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkImage* image, VkDeviceMemory* imageMemory) {
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.extent.width = width;
  imageCreateInfo.extent.height = height;
  imageCreateInfo.extent.depth = 1;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.format = format;
  imageCreateInfo.tiling = tiling;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.usage = usageFlags;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(app->logicalDevice, &imageCreateInfo, NULL, image) == VK_SUCCESS) {
    printf("created image\n");
  }

  VkMemoryRequirements memoryRequirements;
  vkGetImageMemoryRequirements(app->logicalDevice, *image, &memoryRequirements);

  VkMemoryAllocateInfo memoryAllocateInfo = {};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  
  uint32_t memoryTypeIndex = -1;
  for (int x = 0; x < app->memoryProperties.memoryTypeCount; x++) {
    if ((memoryRequirements.memoryTypeBits & (1 << x)) && (app->memoryProperties.memoryTypes[x].propertyFlags & propertyFlags) == propertyFlags) {
      memoryTypeIndex = x;
      break;
    }
  }
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

  if (vkAllocateMemory(app->logicalDevice, &memoryAllocateInfo, NULL, imageMemory) != VK_SUCCESS) {
    printf("allocated image memory\n");
  }

  vkBindImageMemory(app->logicalDevice, *image, *imageMemory, 0);
}

void initializeScene(struct Scene* scene, const char* fileNameOBJ) {
  tinyobj_attrib_init(&scene->attributes);
  tinyobj_parse_obj(&scene->attributes, &scene->shapes, &scene->numShapes, &scene->materials, &scene->numMaterials, fileNameOBJ, readFile, TINYOBJ_FLAG_TRIANGULATE);
}

void initializeVulkanContext(struct VulkanApplication* app) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  app->window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);

  glfwSetInputMode(app->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(app->window, keyCallback);
  
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  uint32_t extensionCount = glfwExtensionCount + 2;
  const char** extensionNames = (const char**)malloc(sizeof(const char*) * extensionCount);
  memcpy(extensionNames, glfwExtensionNames, sizeof(const char*) * glfwExtensionCount); 
  extensionNames[glfwExtensionCount] = "VK_KHR_get_physical_device_properties2";
  extensionNames[glfwExtensionCount + 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.pApplicationName = "vulkan_c_ray_tracing";
  applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.pEngineName = "No Engine";
  applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.apiVersion = VK_API_VERSION_1_2;
  
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.flags = 0;
  instanceCreateInfo.pApplicationInfo = &applicationInfo;
  instanceCreateInfo.enabledExtensionCount = extensionCount;
  instanceCreateInfo.ppEnabledExtensionNames = extensionNames;

  if (ENABLE_VALIDATION) {
    uint32_t layerCount = 1;
    const char** layerNames = (const char**)malloc(sizeof(const char*) * layerCount);
    layerNames[0] = "VK_LAYER_KHRONOS_validation";

    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {};
    messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerCreateInfo.pfnUserCallback = debugCallback;

    instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&messengerCreateInfo;
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.ppEnabledLayerNames = layerNames;

    if (vkCreateInstance(&instanceCreateInfo, NULL, &app->instance) == VK_SUCCESS) {
      printf("created Vulkan instance\n");
    }

    PFN_vkCreateDebugUtilsMessengerEXT pvkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(app->instance, "vkCreateDebugUtilsMessengerEXT");
    if (pvkCreateDebugUtilsMessengerEXT(app->instance, &messengerCreateInfo, NULL, &app->debugMessenger) == VK_SUCCESS) {
      printf("created debug messenger\n");
    }

    free(layerNames);
  }
  else {
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.pNext = NULL;

    if (vkCreateInstance(&instanceCreateInfo, NULL, &app->instance) == VK_SUCCESS) {
      printf("created Vulkan instance\n");
    }
  }

  if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) == VK_SUCCESS) {
    printf("created window surface\n");
  }

  free(extensionNames);
}

void pickPhysicalDevice(struct VulkanApplication* app) {
  app->physicalDevice = VK_NULL_HANDLE;
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(app->instance, &deviceCount, NULL);
  VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * deviceCount);
  vkEnumeratePhysicalDevices(app->instance, &deviceCount, devices);
  app->physicalDevice = devices[0];

  if (app->physicalDevice != VK_NULL_HANDLE) {
    printf("picked physical device\n");
  }

  vkGetPhysicalDeviceMemoryProperties(app->physicalDevice, &app->memoryProperties);

  free(devices);
}

void createLogicalConnection(struct VulkanApplication* app) {
  app->graphicsQueueIndex = -1;
  app->presentQueueIndex = -1; 
  app->computeQueueIndex = -1;
 
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties* queueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, queueFamilyProperties);

  for (int x = 0; x < queueFamilyCount; x++) {
    if (app->graphicsQueueIndex == -1 && queueFamilyProperties[x].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      app->graphicsQueueIndex = x;
    }

    if (app->computeQueueIndex == -1 && queueFamilyProperties[x].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      app->computeQueueIndex = x;
    }

    VkBool32 isPresentSupported = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(app->physicalDevice, x, app->surface, &isPresentSupported);
    
    if (app->presentQueueIndex == -1 && isPresentSupported) {
      app->presentQueueIndex = x;
    }
  
    if (app->graphicsQueueIndex != -1 && app->presentQueueIndex != -1 && app->computeQueueIndex != -1) {
      break;
    }
  }
  
  uint32_t deviceEnabledExtensionCount = 12;
  const char** deviceEnabledExtensionNames = (const char**)malloc(sizeof(const char*) * deviceEnabledExtensionCount);
  deviceEnabledExtensionNames[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;  
  deviceEnabledExtensionNames[1] = "VK_KHR_ray_tracing_pipeline";  
  deviceEnabledExtensionNames[2] = "VK_KHR_acceleration_structure";
  deviceEnabledExtensionNames[3] = "VK_KHR_spirv_1_4";
  deviceEnabledExtensionNames[4] = "VK_KHR_shader_float_controls";
  deviceEnabledExtensionNames[5] = "VK_KHR_get_memory_requirements2";
  deviceEnabledExtensionNames[6] = "VK_EXT_descriptor_indexing";
  deviceEnabledExtensionNames[7] = "VK_KHR_buffer_device_address";
  deviceEnabledExtensionNames[8] = "VK_KHR_deferred_host_operations";
  deviceEnabledExtensionNames[9] = "VK_KHR_pipeline_library";
  deviceEnabledExtensionNames[10] = "VK_KHR_maintenance3";
  deviceEnabledExtensionNames[11] = "VK_KHR_maintenance1";
  
  float queuePriority = 1.0f;
  uint32_t deviceQueueCreateInfoCount = 3;
  VkDeviceQueueCreateInfo* deviceQueueCreateInfos = (VkDeviceQueueCreateInfo*)malloc(sizeof(VkDeviceQueueCreateInfo) * deviceQueueCreateInfoCount);
  
  deviceQueueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfos[0].pNext = NULL;
  deviceQueueCreateInfos[0].flags = 0;
  deviceQueueCreateInfos[0].queueFamilyIndex = app->graphicsQueueIndex;
  deviceQueueCreateInfos[0].queueCount = 1;
  deviceQueueCreateInfos[0].pQueuePriorities = &queuePriority;
 
  deviceQueueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfos[1].pNext = NULL;
  deviceQueueCreateInfos[1].flags = 0;
  deviceQueueCreateInfos[1].queueFamilyIndex = app->presentQueueIndex;
  deviceQueueCreateInfos[1].queueCount = 1;
  deviceQueueCreateInfos[1].pQueuePriorities = &queuePriority;

  deviceQueueCreateInfos[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfos[2].pNext = NULL;
  deviceQueueCreateInfos[2].flags = 0;
  deviceQueueCreateInfos[2].queueFamilyIndex = app->computeQueueIndex;
  deviceQueueCreateInfos[2].queueCount = 1;
  deviceQueueCreateInfos[2].pQueuePriorities = &queuePriority;
 
  VkPhysicalDeviceBufferDeviceAddressFeaturesEXT bufferDeviceAddressFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,
    .pNext = NULL,
    .bufferDeviceAddress = VK_TRUE,
    .bufferDeviceAddressCaptureReplay = VK_FALSE,
    .bufferDeviceAddressMultiDevice = VK_FALSE
  };

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .pNext = &bufferDeviceAddressFeatures,
    .rayTracingPipeline = VK_TRUE
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .pNext = &rayTracingPipelineFeatures,
    .accelerationStructure = VK_TRUE,
    .accelerationStructureCaptureReplay = VK_TRUE,
    .accelerationStructureIndirectBuild = VK_FALSE,
    .accelerationStructureHostCommands = VK_FALSE,
    .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE
  };

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = &accelerationStructureFeatures;
  deviceCreateInfo.flags = 0;
  deviceCreateInfo.queueCreateInfoCount = deviceQueueCreateInfoCount;
  deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos;
  deviceCreateInfo.enabledLayerCount = 0;
  deviceCreateInfo.enabledExtensionCount = deviceEnabledExtensionCount;
  deviceCreateInfo.ppEnabledExtensionNames = deviceEnabledExtensionNames;
  deviceCreateInfo.pEnabledFeatures = NULL;

  if (vkCreateDevice(app->physicalDevice, &deviceCreateInfo, NULL, &app->logicalDevice) == VK_SUCCESS) {
    printf("created logical connection to device\n");
  }

  vkGetDeviceQueue(app->logicalDevice, app->graphicsQueueIndex, 0, &app->graphicsQueue);
  vkGetDeviceQueue(app->logicalDevice, app->presentQueueIndex, 0, &app->presentQueue);
  vkGetDeviceQueue(app->logicalDevice, app->computeQueueIndex, 0, &app->computeQueue);

  free(deviceEnabledExtensionNames);
  free(queueFamilyProperties);
  free(deviceQueueCreateInfos);
}

void createSwapchain(struct VulkanApplication* app) {
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app->physicalDevice, app->surface, &surfaceCapabilities);
  
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(app->physicalDevice, app->surface, &formatCount, NULL);
  VkSurfaceFormatKHR* surfaceFormats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(app->physicalDevice, app->surface, &formatCount, surfaceFormats);

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(app->physicalDevice, app->surface, &presentModeCount, NULL);
  VkPresentModeKHR* surfacePresentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(app->physicalDevice, app->surface, &presentModeCount, surfacePresentModes);

  VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
  VkPresentModeKHR presentMode = surfacePresentModes[0];
  VkExtent2D extent = surfaceCapabilities.currentExtent;

  app->imageCount = surfaceCapabilities.minImageCount + 1;
  if (surfaceCapabilities.maxImageCount > 0 && app->imageCount > surfaceCapabilities.maxImageCount) {
    app->imageCount = surfaceCapabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface = app->surface;
  swapchainCreateInfo.minImageCount = app->imageCount;
  swapchainCreateInfo.imageFormat = surfaceFormat.format;
  swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageExtent = extent;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (app->graphicsQueueIndex != app->presentQueueIndex) {
    uint32_t queueFamilyIndices[2] = {app->graphicsQueueIndex, app->presentQueueIndex};

    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchainCreateInfo.queueFamilyIndexCount = 2;
    swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
  }
  else {
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(app->logicalDevice, &swapchainCreateInfo, NULL, &app->swapchain) == VK_SUCCESS) {
    printf("created swapchain\n");
  }

  vkGetSwapchainImagesKHR(app->logicalDevice, app->swapchain, &app->imageCount, NULL);
  app->swapchainImages = (VkImage*)malloc(sizeof(VkImage) * app->imageCount);
  vkGetSwapchainImagesKHR(app->logicalDevice, app->swapchain, &app->imageCount, app->swapchainImages);

  app->swapchainImageFormat = surfaceFormat.format;

  free(surfaceFormats);
  free(surfacePresentModes);
}

void createCommandPool(struct VulkanApplication* app) {
  VkCommandPoolCreateInfo commandPoolCreateInfo = {};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = app->graphicsQueueIndex;

  if (vkCreateCommandPool(app->logicalDevice, &commandPoolCreateInfo, NULL, &app->commandPool) == VK_SUCCESS) {
    printf("created command pool\n");
  }
}

void createVertexBuffer(struct VulkanApplication* app, struct Scene* scene) {
  VkDeviceSize positionBufferSize = sizeof(float) * scene->attributes.num_vertices * 3;
  
  VkBuffer positionStagingBuffer;
  VkDeviceMemory positionStagingBufferMemory;
  createBuffer(app, positionBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &positionStagingBuffer, &positionStagingBufferMemory);

  void* positionData;
  vkMapMemory(app->logicalDevice, positionStagingBufferMemory, 0, positionBufferSize, 0, &positionData);
  memcpy(positionData, scene->attributes.vertices, positionBufferSize);
  vkUnmapMemory(app->logicalDevice, positionStagingBufferMemory);

  createBuffer(app, positionBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->vertexPositionBuffer, &app->vertexPositionBufferMemory);  

  copyBuffer(app, positionStagingBuffer, app->vertexPositionBuffer, positionBufferSize);

  vkDestroyBuffer(app->logicalDevice, positionStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, positionStagingBufferMemory, NULL);
}

void createIndexBuffer(struct VulkanApplication* app, struct Scene* scene) {
  VkDeviceSize bufferSize = sizeof(uint32_t) * scene->attributes.num_faces;

  uint32_t* positionIndices = (uint32_t*)malloc(bufferSize);
  for (int x = 0; x < scene->attributes.num_faces; x++) {
    positionIndices[x] = scene->attributes.faces[x].v_idx;
  }
  
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  void* data;
  vkMapMemory(app->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, positionIndices, bufferSize);
  vkUnmapMemory(app->logicalDevice, stagingBufferMemory);

  createBuffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->indexBuffer, &app->indexBufferMemory);

  copyBuffer(app, stagingBuffer, app->indexBuffer, bufferSize);
  
  vkDestroyBuffer(app->logicalDevice, stagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, stagingBufferMemory, NULL);

  free(positionIndices);
}

void createMaterialsBuffer(struct VulkanApplication* app, struct Scene* scene) {
  VkDeviceSize indexBufferSize = sizeof(uint32_t) * scene->attributes.num_face_num_verts;

  VkBuffer indexStagingBuffer;
  VkDeviceMemory indexStagingBufferMemory;
  createBuffer(app, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &indexStagingBuffer, &indexStagingBufferMemory);

  void* indexData;
  vkMapMemory(app->logicalDevice, indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
  memcpy(indexData, scene->attributes.material_ids, indexBufferSize);
  vkUnmapMemory(app->logicalDevice, indexStagingBufferMemory);

  createBuffer(app, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->materialIndexBuffer, &app->materialIndexBufferMemory);

  copyBuffer(app, indexStagingBuffer, app->materialIndexBuffer, indexBufferSize);
  
  vkDestroyBuffer(app->logicalDevice, indexStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, indexStagingBufferMemory, NULL);

  VkDeviceSize materialBufferSize = sizeof(struct Material) * scene->numMaterials;

  struct Material* materials = (struct Material*)malloc(materialBufferSize);
  for (int x = 0; x < scene->numMaterials; x++) {
    memcpy(materials[x].ambient, scene->materials[x].ambient, sizeof(float) * 3);
    memcpy(materials[x].diffuse, scene->materials[x].diffuse, sizeof(float) * 3);
    memcpy(materials[x].specular, scene->materials[x].specular, sizeof(float) * 3);
    memcpy(materials[x].emission, scene->materials[x].emission, sizeof(float) * 3);
  }

  VkBuffer materialStagingBuffer;
  VkDeviceMemory materialStagingBufferMemory;
  createBuffer(app, materialBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &materialStagingBuffer, &materialStagingBufferMemory);

  void* materialData;
  vkMapMemory(app->logicalDevice, materialStagingBufferMemory, 0, materialBufferSize, 0, &materialData);
  memcpy(materialData, materials, materialBufferSize);
  vkUnmapMemory(app->logicalDevice, materialStagingBufferMemory);

  createBuffer(app, materialBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->materialBuffer, &app->materialBufferMemory);

  copyBuffer(app, materialStagingBuffer, app->materialBuffer, materialBufferSize);
  
  vkDestroyBuffer(app->logicalDevice, materialStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, materialStagingBufferMemory, NULL);

  free(materials);
}

void createTextures(struct VulkanApplication* app) {
  createImage(app, 800, 600, app->swapchainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->rayTraceImage, &app->rayTraceImageMemory);

  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = 1;
  subresourceRange.baseArrayLayer = 0;
  subresourceRange.layerCount = 1;

  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.pNext = NULL;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCreateInfo.format = app->swapchainImageFormat;
  imageViewCreateInfo.subresourceRange = subresourceRange;
  imageViewCreateInfo.image = app->rayTraceImage;

  if (vkCreateImageView(app->logicalDevice, &imageViewCreateInfo, NULL, &app->rayTraceImageView) == VK_SUCCESS) {
    printf("created image view\n");
  }

  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.pNext = NULL;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.image = app->rayTraceImage;
  imageMemoryBarrier.subresourceRange = subresourceRange;
  imageMemoryBarrier.srcAccessMask = 0;
  imageMemoryBarrier.dstAccessMask = 0;

  VkCommandBufferAllocateInfo bufferAllocateInfo = {};
  bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandPool = app->commandPool;
  bufferAllocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(app->logicalDevice, &bufferAllocateInfo, &commandBuffer);
  
  VkCommandBufferBeginInfo commandBufferBeginInfo = {};
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  
  vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(app->computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(app->computeQueue);

  vkFreeCommandBuffers(app->logicalDevice, app->commandPool, 1, &commandBuffer);
}

void createBottomLevelAccelerationStructure(struct VulkanApplication* app, struct Scene* scene) {
  PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
  PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCreateAccelerationStructureKHR");
  PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetBufferDeviceAddressKHR");
  PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCmdBuildAccelerationStructuresKHR");

  VkBufferDeviceAddressInfo vertexBufferDeviceAddressInfo = {};
  vertexBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  vertexBufferDeviceAddressInfo.buffer = app->vertexPositionBuffer;

  VkDeviceAddress vertexBufferAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &vertexBufferDeviceAddressInfo);

  VkDeviceOrHostAddressConstKHR vertexDeviceOrHostAddressConst = {};
  vertexDeviceOrHostAddressConst.deviceAddress = vertexBufferAddress;

  VkBufferDeviceAddressInfo indexBufferDeviceAddressInfo = {};
  indexBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  indexBufferDeviceAddressInfo.buffer = app->indexBuffer;

  VkDeviceAddress indexBufferAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &indexBufferDeviceAddressInfo);

  VkDeviceOrHostAddressConstKHR indexDeviceOrHostAddressConst = {};
  indexDeviceOrHostAddressConst.deviceAddress = indexBufferAddress;

  VkAccelerationStructureGeometryTrianglesDataKHR accelerationStructureGeometryTrianglesData = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
    .pNext = NULL,
    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
    .vertexData = vertexDeviceOrHostAddressConst,
    .vertexStride = sizeof(float) * 3,
    .maxVertex = scene->attributes.num_vertices,
    .indexType = VK_INDEX_TYPE_UINT32,
    .indexData = indexDeviceOrHostAddressConst,
    .transformData = (VkDeviceOrHostAddressConstKHR){}
  };

  VkAccelerationStructureGeometryDataKHR accelerationStructureGeometryData = {
    .triangles = accelerationStructureGeometryTrianglesData
  };

  VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .pNext = NULL,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .geometry = accelerationStructureGeometryData,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
  };

  VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .pNext = NULL,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = 0,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .srcAccelerationStructure = VK_NULL_HANDLE,
    .dstAccelerationStructure = VK_NULL_HANDLE,
    .geometryCount = 1,
    .pGeometries = &accelerationStructureGeometry,
    .ppGeometries = NULL,
    .scratchData = {}
  };

  VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    .pNext = NULL,
    .accelerationStructureSize = 0,
    .updateScratchSize = 0,
    .buildScratchSize = 0
  };

  pvkGetAccelerationStructureBuildSizesKHR(app->logicalDevice, 
                                           VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR, 
                                           &accelerationStructureBuildGeometryInfo, 
                                           &accelerationStructureBuildGeometryInfo.geometryCount, 
                                           &accelerationStructureBuildSizesInfo);

  createBuffer(app, accelerationStructureBuildSizesInfo.accelerationStructureSize,  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->bottomLevelAccelerationStructureBuffer, &app->bottomLevelAccelerationStructureBufferMemory);

  VkBuffer scratchBuffer;
  VkDeviceMemory scratchBufferMemory;
  createBuffer(app,
               accelerationStructureBuildSizesInfo.buildScratchSize, 
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
               &scratchBuffer, 
               &scratchBufferMemory);


  VkBufferDeviceAddressInfo scratchBufferDeviceAddressInfo = {};
  scratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratchBufferDeviceAddressInfo.buffer = scratchBuffer;

  VkDeviceAddress scratchBufferAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &scratchBufferDeviceAddressInfo);

  VkDeviceOrHostAddressKHR scratchDeviceOrHostAddress = {};
  scratchDeviceOrHostAddress.deviceAddress = scratchBufferAddress;

  accelerationStructureBuildGeometryInfo.scratchData = scratchDeviceOrHostAddress;

  VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .pNext = NULL,
    .createFlags = 0,
    .buffer = app->bottomLevelAccelerationStructureBuffer,
    .offset = 0,
    .size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .deviceAddress = VK_NULL_HANDLE
  };

  pvkCreateAccelerationStructureKHR(app->logicalDevice, &accelerationStructureCreateInfo, NULL, &app->bottomLevelAccelerationStructure);

  accelerationStructureBuildGeometryInfo.dstAccelerationStructure = app->bottomLevelAccelerationStructure;

  const VkAccelerationStructureBuildRangeInfoKHR* accelerationStructureBuildRangeInfo = &(VkAccelerationStructureBuildRangeInfoKHR){
    .primitiveCount = scene->attributes.num_face_num_verts,
    .primitiveOffset = 0,
    .firstVertex = 0,
    .transformOffset = 0
  };
  const VkAccelerationStructureBuildRangeInfoKHR** accelerationStructureBuildRangeInfos = &accelerationStructureBuildRangeInfo;

  VkCommandBufferAllocateInfo bufferAllocateInfo = {};
  bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandPool = app->commandPool;
  bufferAllocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(app->logicalDevice, &bufferAllocateInfo, &commandBuffer);
  
  VkCommandBufferBeginInfo commandBufferBeginInfo = {};
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  
  vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(app->computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(app->computeQueue);

  vkFreeCommandBuffers(app->logicalDevice, app->commandPool, 1, &commandBuffer);

  vkDestroyBuffer(app->logicalDevice, scratchBuffer, NULL);
  vkFreeMemory(app->logicalDevice, scratchBufferMemory, NULL);
}

void createTopLevelAccelerationStructure(struct VulkanApplication* app) {
  PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
  PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCreateAccelerationStructureKHR");
  PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetBufferDeviceAddressKHR");
  PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCmdBuildAccelerationStructuresKHR");
  PFN_vkGetAccelerationStructureDeviceAddressKHR pvkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");

  VkTransformMatrixKHR transformMatrix = {};
  transformMatrix.matrix[0][0] = 1.0;
  transformMatrix.matrix[1][1] = 1.0;
  transformMatrix.matrix[2][2] = 1.0;

  VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo = {};
  accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  accelerationStructureDeviceAddressInfo.accelerationStructure = app->bottomLevelAccelerationStructure;

  VkDeviceAddress accelerationStructureDeviceAddress = pvkGetAccelerationStructureDeviceAddressKHR(app->logicalDevice, &accelerationStructureDeviceAddressInfo);

  VkAccelerationStructureInstanceKHR geometryInstance = {};
  geometryInstance.transform = transformMatrix;
  geometryInstance.instanceCustomIndex = 0;
  geometryInstance.mask = 0xFF;
  geometryInstance.instanceShaderBindingTableRecordOffset = 0;
  geometryInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  geometryInstance.accelerationStructureReference = accelerationStructureDeviceAddress;

  VkDeviceSize geometryInstanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR);
  
  VkBuffer geometryInstanceStagingBuffer;
  VkDeviceMemory geometryInstanceStagingBufferMemory;
  createBuffer(app, geometryInstanceBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &geometryInstanceStagingBuffer, &geometryInstanceStagingBufferMemory);

  void* geometryInstanceData;
  vkMapMemory(app->logicalDevice, geometryInstanceStagingBufferMemory, 0, geometryInstanceBufferSize, 0, &geometryInstanceData);
  memcpy(geometryInstanceData, &geometryInstance, geometryInstanceBufferSize);
  vkUnmapMemory(app->logicalDevice, geometryInstanceStagingBufferMemory);

  VkBuffer geometryInstanceBuffer;
  VkDeviceMemory geometryInstanceBufferMemory;
  createBuffer(app, geometryInstanceBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &geometryInstanceBuffer, &geometryInstanceBufferMemory);  

  copyBuffer(app, geometryInstanceStagingBuffer, geometryInstanceBuffer, geometryInstanceBufferSize);

  vkDestroyBuffer(app->logicalDevice, geometryInstanceStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, geometryInstanceStagingBufferMemory, NULL);

  VkBufferDeviceAddressInfo geometryInstanceBufferDeviceAddressInfo = {};
  geometryInstanceBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  geometryInstanceBufferDeviceAddressInfo.buffer = geometryInstanceBuffer;

  VkDeviceAddress geometryInstanceBufferAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &geometryInstanceBufferDeviceAddressInfo);

  VkDeviceOrHostAddressConstKHR geometryInstanceDeviceOrHostAddressConst = {
    .deviceAddress = geometryInstanceBufferAddress
  };

  VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesData = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
    .pNext = NULL,
    .arrayOfPointers = VK_FALSE,
    .data = geometryInstanceDeviceOrHostAddressConst
  };

  VkAccelerationStructureGeometryDataKHR accelerationStructureGeometryData = {
    .instances = accelerationStructureGeometryInstancesData
  };

  VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .pNext = NULL,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry = accelerationStructureGeometryData,
    .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
  };

  VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .pNext = NULL,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .srcAccelerationStructure = VK_NULL_HANDLE,
    .dstAccelerationStructure = VK_NULL_HANDLE,
    .geometryCount = 1,
    .pGeometries = &accelerationStructureGeometry,
    .ppGeometries = NULL,
    .scratchData = {}
  };

  VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    .pNext = NULL,
    .accelerationStructureSize = 0,
    .updateScratchSize = 0,
    .buildScratchSize = 0
  };

  pvkGetAccelerationStructureBuildSizesKHR(app->logicalDevice, 
                                           VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR, 
                                           &accelerationStructureBuildGeometryInfo, 
                                           &accelerationStructureBuildGeometryInfo.geometryCount, 
                                           &accelerationStructureBuildSizesInfo);

  createBuffer(app, accelerationStructureBuildSizesInfo.accelerationStructureSize,  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->topLevelAccelerationStructureBuffer, &app->topLevelAccelerationStructureBufferMemory);

  VkBuffer scratchBuffer;
  VkDeviceMemory scratchBufferMemory;
  createBuffer(app,
               accelerationStructureBuildSizesInfo.buildScratchSize, 
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
               &scratchBuffer, 
               &scratchBufferMemory);


  VkBufferDeviceAddressInfo scratchBufferDeviceAddressInfo = {};
  scratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  scratchBufferDeviceAddressInfo.buffer = scratchBuffer;

  VkDeviceAddress scratchBufferAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &scratchBufferDeviceAddressInfo);

  VkDeviceOrHostAddressKHR scratchDeviceOrHostAddress = {};
  scratchDeviceOrHostAddress.deviceAddress = scratchBufferAddress;

  accelerationStructureBuildGeometryInfo.scratchData = scratchDeviceOrHostAddress;

  VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .pNext = NULL,
    .createFlags = 0,
    .buffer = app->topLevelAccelerationStructureBuffer,
    .offset = 0,
    .size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    .deviceAddress = VK_NULL_HANDLE
  };

  pvkCreateAccelerationStructureKHR(app->logicalDevice, &accelerationStructureCreateInfo, NULL, &app->topLevelAccelerationStructure);

  accelerationStructureBuildGeometryInfo.dstAccelerationStructure = app->topLevelAccelerationStructure;

  const VkAccelerationStructureBuildRangeInfoKHR* accelerationStructureBuildRangeInfo = &(VkAccelerationStructureBuildRangeInfoKHR){
    .primitiveCount = 1,
    .primitiveOffset = 0,
    .firstVertex = 0,
    .transformOffset = 0
  };
  const VkAccelerationStructureBuildRangeInfoKHR** accelerationStructureBuildRangeInfos = &accelerationStructureBuildRangeInfo;

  VkCommandBufferAllocateInfo bufferAllocateInfo = {};
  bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  bufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  bufferAllocateInfo.commandPool = app->commandPool;
  bufferAllocateInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(app->logicalDevice, &bufferAllocateInfo, &commandBuffer);
  
  VkCommandBufferBeginInfo commandBufferBeginInfo = {};
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  
  vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  pvkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(app->computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(app->computeQueue);

  vkFreeCommandBuffers(app->logicalDevice, app->commandPool, 1, &commandBuffer);

  vkDestroyBuffer(app->logicalDevice, scratchBuffer, NULL);
  vkFreeMemory(app->logicalDevice, scratchBufferMemory, NULL);
}

void createUniformBuffer(struct VulkanApplication* app) {
  VkDeviceSize bufferSize = sizeof(struct Camera);
  createBuffer(app, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &app->uniformBuffer, &app->uniformBufferMemory);
}

void createDescriptorSets(struct VulkanApplication* app) {
  app->rayTraceDescriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * 2);

  VkDescriptorPoolSize descriptorPoolSizes[4];
  descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  descriptorPoolSizes[0].descriptorCount = 1;

  descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descriptorPoolSizes[1].descriptorCount = 1;

  descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorPoolSizes[2].descriptorCount = 1;

  descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorPoolSizes[3].descriptorCount = 4;

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.poolSizeCount = 4;
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
  descriptorPoolCreateInfo.maxSets = 2;

  if (vkCreateDescriptorPool(app->logicalDevice, &descriptorPoolCreateInfo, NULL, &app->descriptorPool) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "created descriptor pool");
  }

  {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[5];
    descriptorSetLayoutBindings[0].binding = 0;
    descriptorSetLayoutBindings[0].descriptorCount = 1;
    descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    descriptorSetLayoutBindings[0].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
   
    descriptorSetLayoutBindings[1].binding = 1;
    descriptorSetLayoutBindings[1].descriptorCount = 1;
    descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorSetLayoutBindings[1].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    descriptorSetLayoutBindings[2].binding = 2;
    descriptorSetLayoutBindings[2].descriptorCount = 1;
    descriptorSetLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorSetLayoutBindings[2].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    descriptorSetLayoutBindings[3].binding = 3;
    descriptorSetLayoutBindings[3].descriptorCount = 1;
    descriptorSetLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[3].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    descriptorSetLayoutBindings[4].binding = 4;
    descriptorSetLayoutBindings[4].descriptorCount = 1;
    descriptorSetLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[4].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
   
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 5;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    
    if (vkCreateDescriptorSetLayout(app->logicalDevice, &descriptorSetLayoutCreateInfo, NULL, &app->rayTraceDescriptorSetLayouts[0]) == VK_SUCCESS) {
      printf("\033[22;32m%s\033[0m\n", "created descriptor set layout");
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = app->descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &app->rayTraceDescriptorSetLayouts[0];

    if (vkAllocateDescriptorSets(app->logicalDevice, &descriptorSetAllocateInfo, &app->rayTraceDescriptorSet) == VK_SUCCESS) {
      printf("\033[22;32m%s\033[0m\n", "allocated descriptor sets");
    }

    VkWriteDescriptorSet writeDescriptorSets[5];

    VkWriteDescriptorSetAccelerationStructureKHR descriptorSetAccelerationStructure = {};
    descriptorSetAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorSetAccelerationStructure.pNext = NULL;
    descriptorSetAccelerationStructure.accelerationStructureCount = 1;
    descriptorSetAccelerationStructure.pAccelerationStructures = &app->topLevelAccelerationStructure;  

    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].pNext = &descriptorSetAccelerationStructure;
    writeDescriptorSets[0].dstSet = app->rayTraceDescriptorSet;
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].dstArrayElement = 0;
    writeDescriptorSets[0].descriptorCount = 1;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    writeDescriptorSets[0].pImageInfo = NULL;
    writeDescriptorSets[0].pBufferInfo = NULL;
    writeDescriptorSets[0].pTexelBufferView = NULL;

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.sampler = VK_DESCRIPTOR_TYPE_SAMPLER;
    imageInfo.imageView = app->rayTraceImageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[1].pNext = NULL;
    writeDescriptorSets[1].dstSet = app->rayTraceDescriptorSet;
    writeDescriptorSets[1].dstBinding = 1;
    writeDescriptorSets[1].dstArrayElement = 0;
    writeDescriptorSets[1].descriptorCount = 1;
    writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeDescriptorSets[1].pImageInfo = &imageInfo;
    writeDescriptorSets[1].pBufferInfo = NULL;
    writeDescriptorSets[1].pTexelBufferView = NULL;

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = app->uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[2].pNext = NULL;
    writeDescriptorSets[2].dstSet = app->rayTraceDescriptorSet;
    writeDescriptorSets[2].dstBinding = 2;
    writeDescriptorSets[2].dstArrayElement = 0;
    writeDescriptorSets[2].descriptorCount = 1;
    writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSets[2].pImageInfo = NULL;
    writeDescriptorSets[2].pBufferInfo = &bufferInfo;
    writeDescriptorSets[2].pTexelBufferView = NULL;

    VkDescriptorBufferInfo indexBufferInfo = {};
    indexBufferInfo.buffer = app->indexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = VK_WHOLE_SIZE;

    writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[3].pNext = NULL;
    writeDescriptorSets[3].dstSet = app->rayTraceDescriptorSet;
    writeDescriptorSets[3].dstBinding = 3;
    writeDescriptorSets[3].dstArrayElement = 0;
    writeDescriptorSets[3].descriptorCount = 1;
    writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[3].pImageInfo = NULL;
    writeDescriptorSets[3].pBufferInfo = &indexBufferInfo;
    writeDescriptorSets[3].pTexelBufferView = NULL;

    VkDescriptorBufferInfo vertexBufferInfo = {};
    vertexBufferInfo.buffer = app->vertexPositionBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = VK_WHOLE_SIZE;

    writeDescriptorSets[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[4].pNext = NULL;
    writeDescriptorSets[4].dstSet = app->rayTraceDescriptorSet;
    writeDescriptorSets[4].dstBinding = 4;
    writeDescriptorSets[4].dstArrayElement = 0;
    writeDescriptorSets[4].descriptorCount = 1;
    writeDescriptorSets[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[4].pImageInfo = NULL;
    writeDescriptorSets[4].pBufferInfo = &vertexBufferInfo;
    writeDescriptorSets[4].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(app->logicalDevice, 5, writeDescriptorSets, 0, NULL);
  }

  {
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2];
    descriptorSetLayoutBindings[0].binding = 0;
    descriptorSetLayoutBindings[0].descriptorCount = 1;
    descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[0].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    descriptorSetLayoutBindings[1].binding = 1;
    descriptorSetLayoutBindings[1].descriptorCount = 1;
    descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorSetLayoutBindings[1].pImmutableSamplers = NULL;
    descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
   
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    
    if (vkCreateDescriptorSetLayout(app->logicalDevice, &descriptorSetLayoutCreateInfo, NULL, &app->rayTraceDescriptorSetLayouts[1]) == VK_SUCCESS) {
      printf("\033[22;32m%s\033[0m\n", "created descriptor set layout");
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = app->descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &app->rayTraceDescriptorSetLayouts[1];

    if (vkAllocateDescriptorSets(app->logicalDevice, &descriptorSetAllocateInfo, &app->materialDescriptorSet) == VK_SUCCESS) {
      printf("\033[22;32m%s\033[0m\n", "allocated descriptor sets");
    }

    VkWriteDescriptorSet writeDescriptorSets[2];

    VkDescriptorBufferInfo materialIndexBufferInfo = {};
    materialIndexBufferInfo.buffer = app->materialIndexBuffer;
    materialIndexBufferInfo.offset = 0;
    materialIndexBufferInfo.range = VK_WHOLE_SIZE;

    writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[0].pNext = NULL;
    writeDescriptorSets[0].dstSet = app->materialDescriptorSet;
    writeDescriptorSets[0].dstBinding = 0;
    writeDescriptorSets[0].dstArrayElement = 0;
    writeDescriptorSets[0].descriptorCount = 1;
    writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[0].pImageInfo = NULL;
    writeDescriptorSets[0].pBufferInfo = &materialIndexBufferInfo;
    writeDescriptorSets[0].pTexelBufferView = NULL;

    VkDescriptorBufferInfo materialBufferInfo = {};
    materialBufferInfo.buffer = app->materialBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[1].pNext = NULL;
    writeDescriptorSets[1].dstSet = app->materialDescriptorSet;
    writeDescriptorSets[1].dstBinding = 1;
    writeDescriptorSets[1].dstArrayElement = 0;
    writeDescriptorSets[1].descriptorCount = 1;
    writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSets[1].pImageInfo = NULL;
    writeDescriptorSets[1].pBufferInfo = &materialBufferInfo;
    writeDescriptorSets[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(app->logicalDevice, 2, writeDescriptorSets, 0, NULL);
  }
}

void createRayTracePipeline(struct VulkanApplication* app) {
  PFN_vkCreateRayTracingPipelinesKHR pvkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCreateRayTracingPipelinesKHR");

  FILE* rgenFile = fopen("bin/raytrace.rgen.spv", "rb");
  fseek(rgenFile, 0, SEEK_END);
  uint32_t rgenFileSize = ftell(rgenFile);
  fseek(rgenFile, 0, SEEK_SET);

  char* rgenFileBuffer = (char*)malloc(sizeof(char*) * rgenFileSize);
  fread(rgenFileBuffer, 1, rgenFileSize, rgenFile);
  fclose(rgenFile);

  FILE* rmissFile = fopen("bin/raytrace.rmiss.spv", "rb");
  fseek(rmissFile, 0, SEEK_END);
  uint32_t rmissFileSize = ftell(rmissFile);
  fseek(rmissFile, 0, SEEK_SET);

  char* rmissFileBuffer = (char*)malloc(sizeof(char*) * rmissFileSize);
  fread(rmissFileBuffer, 1, rmissFileSize, rmissFile);
  fclose(rmissFile);

  FILE* rmissShadowFile = fopen("bin/raytrace_shadow.rmiss.spv", "rb");
  fseek(rmissShadowFile, 0, SEEK_END);
  uint32_t rmissShadowFileSize = ftell(rmissShadowFile);
  fseek(rmissShadowFile, 0, SEEK_SET);

  char* rmissShadowFileBuffer = (char*)malloc(sizeof(char*) * rmissShadowFileSize);
  fread(rmissShadowFileBuffer, 1, rmissShadowFileSize, rmissShadowFile);
  fclose(rmissShadowFile);

  FILE* rchitFile = fopen("bin/raytrace.rchit.spv", "rb");
  fseek(rchitFile, 0, SEEK_END);
  uint32_t rchitFileSize = ftell(rchitFile);
  fseek(rchitFile, 0, SEEK_SET);

  char* rchitFileBuffer = (char*)malloc(sizeof(char*) * rchitFileSize);
  fread(rchitFileBuffer, 1, rchitFileSize, rchitFile);
  fclose(rchitFile);

  VkShaderModuleCreateInfo rgenShaderModuleCreateInfo = {};
  rgenShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  rgenShaderModuleCreateInfo.codeSize = rgenFileSize;
  rgenShaderModuleCreateInfo.pCode = (uint32_t*)rgenFileBuffer;
  
  VkShaderModule rgenShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &rgenShaderModuleCreateInfo, NULL, &rgenShaderModule) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "created rgen shader module");
  }

  VkShaderModuleCreateInfo rmissShaderModuleCreateInfo = {};
  rmissShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  rmissShaderModuleCreateInfo.codeSize = rmissFileSize;
  rmissShaderModuleCreateInfo.pCode = (uint32_t*)rmissFileBuffer;

  VkShaderModule rmissShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &rmissShaderModuleCreateInfo, NULL, &rmissShaderModule) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "created rmiss shader module");
  }

  VkShaderModuleCreateInfo rmissShadowShaderModuleCreateInfo = {};
  rmissShadowShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  rmissShadowShaderModuleCreateInfo.codeSize = rmissShadowFileSize;
  rmissShadowShaderModuleCreateInfo.pCode = (uint32_t*)rmissShadowFileBuffer;

  VkShaderModule rmissShadowShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &rmissShadowShaderModuleCreateInfo, NULL, &rmissShadowShaderModule) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "created rmiss shadow shader module");
  }

  VkShaderModuleCreateInfo rchitShaderModuleCreateInfo = {};
  rchitShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  rchitShaderModuleCreateInfo.codeSize = rchitFileSize;
  rchitShaderModuleCreateInfo.pCode = (uint32_t*)rchitFileBuffer;

  VkShaderModule rchitShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &rchitShaderModuleCreateInfo, NULL, &rchitShaderModule) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "created rchit shader module");
  }
 
  VkPipelineShaderStageCreateInfo rgenShaderStageInfo = {};
  rgenShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  rgenShaderStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  rgenShaderStageInfo.module = rgenShaderModule;
  rgenShaderStageInfo.pName = "main";
  
  VkPipelineShaderStageCreateInfo rmissShaderStageInfo = {};
  rmissShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  rmissShaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
  rmissShaderStageInfo.module = rmissShaderModule;
  rmissShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo rmissShadowShaderStageInfo = {};
  rmissShadowShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  rmissShadowShaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
  rmissShadowShaderStageInfo.module = rmissShadowShaderModule;
  rmissShadowShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo rchitShaderStageInfo = {};
  rchitShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  rchitShaderStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  rchitShaderStageInfo.module = rchitShaderModule;
  rchitShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[4] = {rgenShaderStageInfo, rmissShaderStageInfo, rmissShadowShaderStageInfo, rchitShaderStageInfo};

  VkRayTracingShaderGroupCreateInfoKHR shaderGroupCreateInfos[4];
  shaderGroupCreateInfos[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shaderGroupCreateInfos[0].pNext = NULL;
  shaderGroupCreateInfos[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  shaderGroupCreateInfos[0].generalShader = 0;
  shaderGroupCreateInfos[0].closestHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[0].anyHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[0].intersectionShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[0].pShaderGroupCaptureReplayHandle = NULL;

  shaderGroupCreateInfos[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shaderGroupCreateInfos[1].pNext = NULL;
  shaderGroupCreateInfos[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  shaderGroupCreateInfos[1].generalShader = 1;
  shaderGroupCreateInfos[1].closestHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[1].anyHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[1].intersectionShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[1].pShaderGroupCaptureReplayHandle = NULL;

  shaderGroupCreateInfos[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shaderGroupCreateInfos[2].pNext = NULL;
  shaderGroupCreateInfos[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  shaderGroupCreateInfos[2].generalShader = 2;
  shaderGroupCreateInfos[2].closestHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[2].anyHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[2].intersectionShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[2].pShaderGroupCaptureReplayHandle = NULL;

  shaderGroupCreateInfos[3].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
  shaderGroupCreateInfos[3].pNext = NULL;
  shaderGroupCreateInfos[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  shaderGroupCreateInfos[3].generalShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[3].closestHitShader = 3;
  shaderGroupCreateInfos[3].anyHitShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[3].intersectionShader = VK_SHADER_UNUSED_KHR;
  shaderGroupCreateInfos[3].pShaderGroupCaptureReplayHandle = NULL;
  
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 2;
  pipelineLayoutCreateInfo.pSetLayouts = app->rayTraceDescriptorSetLayouts;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

  if (vkCreatePipelineLayout(app->logicalDevice, &pipelineLayoutCreateInfo, NULL, &app->rayTracePipelineLayout) == VK_SUCCESS) {
    printf("created ray trace pipline layout\n");
  }

  VkPipelineLibraryCreateInfoKHR pipelineLibraryCreateInfo = {};
  pipelineLibraryCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;
  pipelineLibraryCreateInfo.pNext = NULL;
  pipelineLibraryCreateInfo.libraryCount = 0;
  pipelineLibraryCreateInfo.pLibraries = NULL;

  VkRayTracingPipelineCreateInfoKHR rayPipelineCreateInfo = {};
  rayPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
  rayPipelineCreateInfo.pNext = NULL;
  rayPipelineCreateInfo.flags = 0;
  rayPipelineCreateInfo.stageCount = 4;
  rayPipelineCreateInfo.pStages = shaderStages;
  rayPipelineCreateInfo.groupCount = 4;
  rayPipelineCreateInfo.pGroups = shaderGroupCreateInfos;
  rayPipelineCreateInfo.maxPipelineRayRecursionDepth = 16;
  rayPipelineCreateInfo.pLibraryInfo = &pipelineLibraryCreateInfo;
  rayPipelineCreateInfo.pLibraryInterface = NULL;
  rayPipelineCreateInfo.layout = app->rayTracePipelineLayout;
  rayPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
  rayPipelineCreateInfo.basePipelineIndex = -1;

  if (pvkCreateRayTracingPipelinesKHR(app->logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineCreateInfo, NULL, &app->rayTracePipeline) == VK_SUCCESS) {
    printf("created ray trace pipeline\n");
  }

  vkDestroyShaderModule(app->logicalDevice, rgenShaderModule, NULL);
  vkDestroyShaderModule(app->logicalDevice, rmissShaderModule, NULL);
  vkDestroyShaderModule(app->logicalDevice, rmissShadowShaderModule, NULL);
  vkDestroyShaderModule(app->logicalDevice, rchitShaderModule, NULL);

  free(rgenFileBuffer);
  free(rmissFileBuffer);
  free(rmissShadowFileBuffer);
  free(rchitFileBuffer);
}

void createShaderBindingTable(struct VulkanApplication* app) {
  PFN_vkGetRayTracingShaderGroupHandlesKHR pvkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties = {};
  rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

  VkPhysicalDeviceProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &rayTracingProperties;
  
  vkGetPhysicalDeviceProperties2(app->physicalDevice, &properties);
  
  VkDeviceSize shaderBindingTableSize = rayTracingProperties.shaderGroupHandleSize * 4;

  app->shaderBindingTableBufferMemory;
  createBuffer(app, shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &app->shaderBindingTableBuffer, &app->shaderBindingTableBufferMemory);

  void* shaderHandleStorage = (void*)malloc(sizeof(uint8_t) * shaderBindingTableSize);
  if (pvkGetRayTracingShaderGroupHandlesKHR(app->logicalDevice, app->rayTracePipeline, 0, 4, shaderBindingTableSize, shaderHandleStorage) == VK_SUCCESS) {
    printf("got ray tracing shader group handles\n");
  }

  void* data;
  vkMapMemory(app->logicalDevice, app->shaderBindingTableBufferMemory, 0, shaderBindingTableSize, 0, &data);
  for (int x = 0; x < 4; x++) {
    memcpy(data, (uint8_t*)shaderHandleStorage + x * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
    data += rayTracingProperties.shaderGroupBaseAlignment;
  }
  vkUnmapMemory(app->logicalDevice, app->shaderBindingTableBufferMemory);

  free(shaderHandleStorage);
}

void createCommandBuffers(struct VulkanApplication* app) {
  PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkGetBufferDeviceAddressKHR");
  PFN_vkCmdTraceRaysKHR pvkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkCmdTraceRaysKHR");

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties = {};
  rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

  VkPhysicalDeviceProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &rayTracingProperties;
  
  vkGetPhysicalDeviceProperties2(app->physicalDevice, &properties);

  app->commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * app->imageCount);
  
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = app->commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = app->imageCount;

  if (vkAllocateCommandBuffers(app->logicalDevice, &commandBufferAllocateInfo, app->commandBuffers) == VK_SUCCESS) {
    printf("\033[22;32m%s\033[0m\n", "allocated command buffers");
  }

  for (int x = 0; x < app->imageCount; x++) {
    VkCommandBufferBeginInfo commandBufferBeginCreateInfo = {};
    commandBufferBeginCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkDeviceSize progSize = rayTracingProperties.shaderGroupBaseAlignment;
    VkDeviceSize sbtSize = progSize * (VkDeviceSize)4;
    VkDeviceSize rayGenOffset = 0u * progSize;
    VkDeviceSize missOffset = 1u * progSize;
    VkDeviceSize hitGroupOffset = 3u * progSize;

    VkBufferDeviceAddressInfo shaderBindingTableBufferDeviceAddressInfo = {};
    shaderBindingTableBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    shaderBindingTableBufferDeviceAddressInfo.buffer = app->shaderBindingTableBuffer;

    VkDeviceAddress shaderBindingTableBufferDeviceAddress = pvkGetBufferDeviceAddressKHR(app->logicalDevice, &shaderBindingTableBufferDeviceAddressInfo);

    const VkStridedDeviceAddressRegionKHR rgenShaderBindingTable = {
      .deviceAddress = shaderBindingTableBufferDeviceAddress + 0u * progSize,
      .stride = sbtSize,
      .size = sbtSize * 1
    };

    const VkStridedDeviceAddressRegionKHR rmissShaderBindingTable = {
      .deviceAddress = shaderBindingTableBufferDeviceAddress + 1u * progSize,
      .stride = progSize,
      .size = sbtSize * 2 
    };

    const VkStridedDeviceAddressRegionKHR rchitShaderBindingTable = {
      .deviceAddress = shaderBindingTableBufferDeviceAddress + 3u * progSize,
      .stride = progSize,
      .size = sbtSize * 1
    };

    const VkStridedDeviceAddressRegionKHR callableShaderBindingTable = {};

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    if (vkBeginCommandBuffer(app->commandBuffers[x], &commandBufferBeginCreateInfo) == VK_SUCCESS) {
      printf("\033[22;32m%s%d\033[0m\n", "began recording command buffer for image #", x);
    }
      
    vkCmdBindPipeline(app->commandBuffers[x], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, app->rayTracePipeline);
    vkCmdBindDescriptorSets(app->commandBuffers[x], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, app->rayTracePipelineLayout, 0, 1, &app->rayTraceDescriptorSet, 0, 0);
    vkCmdBindDescriptorSets(app->commandBuffers[x], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, app->rayTracePipelineLayout, 1, 1, &app->materialDescriptorSet, 0, 0);

    pvkCmdTraceRaysKHR(app->commandBuffers[x], &rgenShaderBindingTable, &rmissShaderBindingTable, &rchitShaderBindingTable, &callableShaderBindingTable, 800, 600, 1);
  
    { 
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      imageMemoryBarrier.image = app->rayTraceImage;
      imageMemoryBarrier.subresourceRange = subresourceRange;
      imageMemoryBarrier.srcAccessMask = 0;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

      vkCmdPipelineBarrier(app->commandBuffers[x], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }

    { 
      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      imageMemoryBarrier.image = app->swapchainImages[x];
      imageMemoryBarrier.subresourceRange = subresourceRange;
      imageMemoryBarrier.srcAccessMask = 0;
      imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      vkCmdPipelineBarrier(app->commandBuffers[x], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }

    {
      VkImageSubresourceLayers subresourceLayers = {};
      subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceLayers.mipLevel = 0;
      subresourceLayers.baseArrayLayer = 0;
      subresourceLayers.layerCount = 1;

      VkOffset3D offset = {};
      offset.x = 0;
      offset.y = 0;
      offset.z = 0;

      VkExtent3D extent = {};
      extent.width = 800;
      extent.height = 600;
      extent.depth = 1;

      VkImageCopy imageCopy = {};
      imageCopy.srcSubresource = subresourceLayers;
      imageCopy.srcOffset = offset;
      imageCopy.dstSubresource = subresourceLayers;
      imageCopy.dstOffset = offset;
      imageCopy.extent = extent;
  
      vkCmdCopyImage(app->commandBuffers[x], app->rayTraceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, app->swapchainImages[x], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
    }

    { 
      VkImageSubresourceRange subresourceRange = {};
      subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceRange.baseMipLevel = 0;
      subresourceRange.levelCount = 1;
      subresourceRange.baseArrayLayer = 0;
      subresourceRange.layerCount = 1;

      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
      imageMemoryBarrier.image = app->rayTraceImage;
      imageMemoryBarrier.subresourceRange = subresourceRange;
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      imageMemoryBarrier.dstAccessMask = 0;

      vkCmdPipelineBarrier(app->commandBuffers[x], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }

    { 
      VkImageSubresourceRange subresourceRange = {};
      subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      subresourceRange.baseMipLevel = 0;
      subresourceRange.levelCount = 1;
      subresourceRange.baseArrayLayer = 0;
      subresourceRange.layerCount = 1;

      VkImageMemoryBarrier imageMemoryBarrier = {};
      imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      imageMemoryBarrier.pNext = NULL;
      imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      imageMemoryBarrier.image = app->swapchainImages[x];
      imageMemoryBarrier.subresourceRange = subresourceRange;
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      imageMemoryBarrier.dstAccessMask = 0;

      vkCmdPipelineBarrier(app->commandBuffers[x], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);
    }

    if (vkEndCommandBuffer(app->commandBuffers[x]) == VK_SUCCESS) {
      printf("\033[22;32m%s%d\033[0m\n", "ended recording command buffer for image #", x);
    }
  }
}

void createSynchronizationObjects(struct VulkanApplication* app) {
  app->imageAvailableSemaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  app->renderFinishedSemaphores = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  app->inFlightFences = (VkFence*)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
  app->imagesInFlight = (VkFence*)malloc(sizeof(VkFence) * app->imageCount);
  for (int x = 0; x < app->imageCount; x++) {
    app->imagesInFlight[x] = VK_NULL_HANDLE;
  }

  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int x = 0; x < MAX_FRAMES_IN_FLIGHT; x++) {
    if (vkCreateSemaphore(app->logicalDevice, &semaphoreCreateInfo, NULL, &app->imageAvailableSemaphores[x]) == VK_SUCCESS &&
        vkCreateSemaphore(app->logicalDevice, &semaphoreCreateInfo, NULL, &app->renderFinishedSemaphores[x]) == VK_SUCCESS &&
        vkCreateFence(app->logicalDevice, &fenceCreateInfo, NULL, &app->inFlightFences[x]) == VK_SUCCESS) {
      printf("created synchronization objects for frame #%d\n", x);
    }
  }
}

void updateUniformBuffer(struct VulkanApplication* app, struct Camera* camera) {
  void* data;
  vkMapMemory(app->logicalDevice, app->uniformBufferMemory, 0, sizeof(struct Camera), 0, &data);
  memcpy(data, camera, sizeof(struct Camera));
  vkUnmapMemory(app->logicalDevice, app->uniformBufferMemory);
}

void drawFrame(struct VulkanApplication* app, struct Camera* camera) {
  vkWaitForFences(app->logicalDevice, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX);
    
  uint32_t imageIndex;
  vkAcquireNextImageKHR(app->logicalDevice, app->swapchain, UINT64_MAX, app->imageAvailableSemaphores[app->currentFrame], VK_NULL_HANDLE, &imageIndex);
    
  if (app->imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(app->logicalDevice, 1, &app->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  app->imagesInFlight[imageIndex] = app->inFlightFences[app->currentFrame];
 
  updateUniformBuffer(app, camera);
   
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
  VkSemaphore waitSemaphores[1] = {app->imageAvailableSemaphores[app->currentFrame]};
  VkPipelineStageFlags waitStages[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &app->commandBuffers[imageIndex];

  VkSemaphore signalSemaphores[1] = {app->renderFinishedSemaphores[app->currentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkResetFences(app->logicalDevice, 1, &app->inFlightFences[app->currentFrame]);

  if (vkQueueSubmit(app->graphicsQueue, 1, &submitInfo, app->inFlightFences[app->currentFrame]) != VK_SUCCESS) {
    printf("failed to submit draw command buffer\n");
  }

  VkPresentInfoKHR presentInfo = {};  
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapchains[1] = {app->swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  vkQueuePresentKHR(app->presentQueue, &presentInfo);

  app->currentFrame = (app->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void runMainLoop(struct VulkanApplication* app, struct Camera* camera) {
  while (!glfwWindowShouldClose(app->window)) {
    glfwPollEvents();

    int isCameraMoved = 0;

    if (keyDownIndex[GLFW_KEY_W]) {
      cameraPosition[0] += cos(-cameraYaw - (M_PI / 2)) * 0.1f;
      cameraPosition[2] += sin(-cameraYaw - (M_PI / 2)) * 0.1f;
      isCameraMoved = 1;
    }
    if (keyDownIndex[GLFW_KEY_S]) {
      cameraPosition[0] -= cos(-cameraYaw - (M_PI / 2)) * 0.1f;
      cameraPosition[2] -= sin(-cameraYaw - (M_PI / 2)) * 0.1f;
      isCameraMoved = 1;
    }
    if (keyDownIndex[GLFW_KEY_A]) {
      cameraPosition[0] -= cos(-cameraYaw) * 0.1f;
      cameraPosition[2] -= sin(-cameraYaw) * 0.1f;
      isCameraMoved = 1;
    }
    if (keyDownIndex[GLFW_KEY_D]) {
      cameraPosition[0] += cos(-cameraYaw) * 0.1f;
      cameraPosition[2] += sin(-cameraYaw) * 0.1f;
      isCameraMoved = 1;
    }
    if (keyDownIndex[GLFW_KEY_SPACE]) {
      cameraPosition[1] += 0.1f;
      isCameraMoved = 1;
    }
    if (keyDownIndex[GLFW_KEY_LEFT_CONTROL]) {
      cameraPosition[1] -= 0.1f;
      isCameraMoved = 1;
    }

    static double previousMousePositionX;
    static double previousMousePositionY;

    double xPos, yPos;
    glfwGetCursorPos(app->window, &xPos, &yPos);

    if (previousMousePositionX != xPos || previousMousePositionY != yPos) {
      double mouseDifferenceX = previousMousePositionX - xPos;
      double mouseDifferenceY = previousMousePositionY - yPos;

      cameraYaw += mouseDifferenceX * 0.0005f;

      previousMousePositionX = xPos;
      previousMousePositionY = yPos;

      isCameraMoved = 1;
    }

    camera->position[0] = cameraPosition[0]; camera->position[1] = cameraPosition[1]; camera->position[2] = cameraPosition[2];

    camera->forward[0] = cosf(cameraPitch) * cosf(-cameraYaw - (M_PI / 2.0));
    camera->forward[1] = sinf(cameraPitch);
    camera->forward[2] = cosf(cameraPitch) * sinf(-cameraYaw - (M_PI / 2.0));

    camera->right[0] = camera->forward[1] * camera->up[2] - camera->forward[2] * camera->up[1];
    camera->right[1] = camera->forward[2] * camera->up[0] - camera->forward[0] * camera->up[2];
    camera->right[2] = camera->forward[0] * camera->up[1] - camera->forward[1] * camera->up[0];

    if (isCameraMoved == 1) {
      camera->frameCount = 0;
    }
    else {
      camera->frameCount += 1;
    }
    
    drawFrame(app, camera);
  }

  vkDeviceWaitIdle(app->logicalDevice);
}

void cleanUp(struct VulkanApplication* app, struct Scene* scene) {
  PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(app->logicalDevice, "vkDestroyAccelerationStructureKHR");
  PFN_vkDestroyDebugUtilsMessengerEXT pvkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(app->instance, "vkDestroyDebugUtilsMessengerEXT");

  for (size_t x = 0; x < MAX_FRAMES_IN_FLIGHT; x++) {
    vkDestroySemaphore(app->logicalDevice, app->renderFinishedSemaphores[x], NULL);
    vkDestroySemaphore(app->logicalDevice, app->imageAvailableSemaphores[x], NULL);
    vkDestroyFence(app->logicalDevice, app->inFlightFences[x], NULL);
  }

  free(app->imageAvailableSemaphores);
  free(app->renderFinishedSemaphores);
  free(app->inFlightFences);
  free(app->imagesInFlight);

  vkFreeCommandBuffers(app->logicalDevice, app->commandPool, app->imageCount, app->commandBuffers);
  free(app->commandBuffers);

  vkDestroyBuffer(app->logicalDevice, app->shaderBindingTableBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->shaderBindingTableBufferMemory, NULL);

  vkDestroyPipeline(app->logicalDevice, app->rayTracePipeline, NULL);
  vkDestroyPipelineLayout(app->logicalDevice, app->rayTracePipelineLayout, NULL);

  vkDestroyDescriptorSetLayout(app->logicalDevice, app->rayTraceDescriptorSetLayouts[1], NULL);
  vkDestroyDescriptorSetLayout(app->logicalDevice, app->rayTraceDescriptorSetLayouts[0], NULL);
  free(app->rayTraceDescriptorSetLayouts);
  vkDestroyDescriptorPool(app->logicalDevice, app->descriptorPool, NULL);

  vkDestroyBuffer(app->logicalDevice, app->uniformBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->uniformBufferMemory, NULL);

  pvkDestroyAccelerationStructureKHR(app->logicalDevice, app->topLevelAccelerationStructure, NULL);
  vkDestroyBuffer(app->logicalDevice, app->topLevelAccelerationStructureBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->topLevelAccelerationStructureBufferMemory, NULL);

  pvkDestroyAccelerationStructureKHR(app->logicalDevice, app->bottomLevelAccelerationStructure, NULL);
  vkDestroyBuffer(app->logicalDevice, app->bottomLevelAccelerationStructureBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->bottomLevelAccelerationStructureBufferMemory, NULL);

  vkDestroyImageView(app->logicalDevice, app->rayTraceImageView, NULL);
  vkFreeMemory(app->logicalDevice, app->rayTraceImageMemory, NULL);
  vkDestroyImage(app->logicalDevice, app->rayTraceImage, NULL);

  vkDestroyBuffer(app->logicalDevice, app->materialBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->materialBufferMemory, NULL);

  vkDestroyBuffer(app->logicalDevice, app->materialIndexBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->materialIndexBufferMemory, NULL);

  vkDestroyBuffer(app->logicalDevice, app->indexBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->indexBufferMemory, NULL);

  vkDestroyBuffer(app->logicalDevice, app->vertexPositionBuffer, NULL);
  vkFreeMemory(app->logicalDevice, app->vertexPositionBufferMemory, NULL);

  vkDestroyCommandPool(app->logicalDevice, app->commandPool, NULL);

  free(app->swapchainImages);
  vkDestroySwapchainKHR(app->logicalDevice, app->swapchain, NULL);

  vkDestroyDevice(app->logicalDevice, NULL);

  if (ENABLE_VALIDATION) {
    pvkDestroyDebugUtilsMessengerEXT(app->instance, app->debugMessenger, NULL);
  }

  vkDestroySurfaceKHR(app->instance, app->surface, NULL);
  vkDestroyInstance(app->instance, NULL);
  glfwDestroyWindow(app->window);
  glfwTerminate();

  tinyobj_attrib_free(&scene->attributes);
  tinyobj_shapes_free(scene->shapes, scene->numShapes);
  tinyobj_materials_free(scene->materials, scene->numMaterials);
}

int main(void) {
  struct VulkanApplication* app = (struct VulkanApplication*)malloc(sizeof(struct VulkanApplication));
  struct Scene* scene = (struct Scene*)malloc(sizeof(struct Scene));

  struct Camera* camera = &(struct Camera) {
    .position = {
      0, 0, 0, 1
    },
    .right = {
      1, 0, 0, 1
    },
    .up = {
      0, 1, 0, 1
    },
    .forward = {
      0, 0, 1, 1
    },

    .frameCount = 0,
  };

  initializeScene(scene, "res/cube_scene.obj");

  initializeVulkanContext(app);
  pickPhysicalDevice(app);
  createLogicalConnection(app);
  createSwapchain(app);  
  createCommandPool(app);
  createVertexBuffer(app, scene);
  createIndexBuffer(app, scene);
  createMaterialsBuffer(app, scene);
  createTextures(app);

  createBottomLevelAccelerationStructure(app, scene);
  createTopLevelAccelerationStructure(app);

  createUniformBuffer(app);
  createDescriptorSets(app);

  createRayTracePipeline(app);
  createShaderBindingTable(app);
  createCommandBuffers(app);

  createSynchronizationObjects(app);

  runMainLoop(app, camera);

  cleanUp(app, scene);

  free(app);
  free(scene);
  
  return 0;
}
