#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MAX_FRAMES_IN_FLIGHT      2

const float verticesPosition[] = {
  -0.5f, -0.5f,
   0.5f, -0.5f,
   0.5f,  0.5f,
  -0.5f,  0.5f,
};

const float verticesColor[] = {
  1.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f,
};

const uint32_t vertexIndices[] = {
  0, 1, 2, 2, 3, 0
};

struct VulkanApplication {
  GLFWwindow* window;
  VkSurfaceKHR surface;
  VkInstance instance;

  VkPhysicalDevice physicalDevice;
  VkDevice logicalDevice;

  uint32_t graphicsQueueIndex;
  uint32_t presentQueueIndex;
  VkQueue graphicsQueue;
  VkQueue presentQueue;

  uint32_t imageCount;
  VkSwapchainKHR swapchain;
  VkImage* swapchainImages;
  VkFormat swapchainImageFormat;
  VkExtent2D swapchainExtent;
  VkImageView* swapchainImageViews;  
  VkFramebuffer* swapchainFramebuffers;
 
  VkRenderPass renderPass; 
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;

  VkCommandPool commandPool;
  VkCommandBuffer* commandBuffers;

  VkSemaphore* imageAvailableSemaphores;
  VkSemaphore* renderFinishedSemaphores;
  VkFence* inFlightFences;
  VkFence* imagesInFlight;
  uint32_t currentFrame;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet* descriptorSets;

  VkBuffer* uniformBuffers;
  VkDeviceMemory* uniformBuffersMemory;

  VkVertexInputBindingDescription* vertexBindingDescriptions;
  VkVertexInputAttributeDescription* vertexAttributeDescriptions;  

  VkPhysicalDeviceMemoryProperties memoryProperties;

  VkBuffer vertexPositionBuffer;
  VkDeviceMemory vertexPositionBufferMemory;
  
  VkBuffer vertexColorBuffer;
  VkDeviceMemory vertexColorBufferMemory;

  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;
};

struct UniformBufferObject {
  float uniformTest;
};

void initializeVulkanContext(struct VulkanApplication* app) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  app->window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
  
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.flags = 0;
  instanceCreateInfo.pApplicationInfo = NULL;
  instanceCreateInfo.enabledLayerCount = 0;
  instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
  instanceCreateInfo.ppEnabledExtensionNames = glfwExtensionNames;

  if (vkCreateInstance(&instanceCreateInfo, NULL, &app->instance) == VK_SUCCESS) {
    printf("created Vulkan instance\n");
  };

  if (glfwCreateWindowSurface(app->instance, app->window, NULL, &app->surface) == VK_SUCCESS) {
    printf("created window surface\n");
  }
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
}

void createLogicalConnection(struct VulkanApplication* app) {
  app->graphicsQueueIndex = -1;
  app->presentQueueIndex = -1; 
 
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, NULL);
  VkQueueFamilyProperties* queueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(app->physicalDevice, &queueFamilyCount, queueFamilyProperties);

  for (int x = 0; x < queueFamilyCount; x++) {
    if (queueFamilyProperties[x].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      app->graphicsQueueIndex = x;
    }

    VkBool32 isPresentSupported = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(app->physicalDevice, x, app->surface, &isPresentSupported);
    
    if (isPresentSupported) {
      app->presentQueueIndex = x;
    }
  
    if (app->graphicsQueueIndex != -1) {
      break;
    }
  }
  
  uint32_t deviceEnabledExtensionCount = 1;
  const char** deviceEnabledExtensionNames = (const char**)malloc(sizeof(const char*) * deviceEnabledExtensionCount);
  deviceEnabledExtensionNames[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;  
  
  float queuePriority = 1.0f;
  uint32_t deviceQueueCreateInfoCount = 2;
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
 
  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.pNext = NULL;
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
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
  app->swapchainExtent = extent;

  app->swapchainImageViews = (VkImageView*)malloc(sizeof(VkImageView) * app->imageCount);

  for (int x = 0; x < app->imageCount; x++) {
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = app->swapchainImages[x];
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = app->swapchainImageFormat;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(app->logicalDevice, &imageViewCreateInfo, NULL, &app->swapchainImageViews[x]) == VK_SUCCESS) {
      printf("created image view #%d\n", x);
    }
  }
}

void createRenderPass(struct VulkanApplication* app) {
  VkAttachmentDescription attachmentDescription = {};
  attachmentDescription.format = app->swapchainImageFormat;
  attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference attachmentReference = {};
  attachmentReference.attachment = 0;
  attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &attachmentReference;

  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = 1;
  renderPassCreateInfo.pAttachments = &attachmentDescription;
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDescription;

  if (vkCreateRenderPass(app->logicalDevice, &renderPassCreateInfo, NULL, &app->renderPass) == VK_SUCCESS) {
    printf("created render pass\n");
  }
}

void createDescriptorSetLayout(struct VulkanApplication* app) {
  VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
  descriptorSetLayoutBinding.binding = 0;
  descriptorSetLayoutBinding.descriptorCount = 1;
  descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorSetLayoutBinding.pImmutableSamplers = NULL;
  descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
  descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCreateInfo.bindingCount = 1;
  descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

  if (vkCreateDescriptorSetLayout(app->logicalDevice, &descriptorSetLayoutCreateInfo, NULL, &app->descriptorSetLayout) == VK_SUCCESS) {
    printf("created descriptor set layout\n");
  }
}

void createGraphicsPipeline(struct VulkanApplication* app) {
  FILE* vertexFile = fopen("bin/basic.vert.spv", "rb");
  fseek(vertexFile, 0, SEEK_END);
  uint32_t vertexFileSize = ftell(vertexFile);
  fseek(vertexFile, 0, SEEK_SET);

  char* vertexFileBuffer = (char*)malloc(sizeof(char*) * vertexFileSize);
  fread(vertexFileBuffer, 1, vertexFileSize, vertexFile);
  fclose(vertexFile);

  FILE* fragmentFile = fopen("bin/basic.frag.spv", "rb");
  fseek(fragmentFile, 0, SEEK_END);
  uint32_t fragmentFileSize = ftell(fragmentFile);
  fseek(fragmentFile, 0, SEEK_SET);

  char* fragmentFileBuffer = (char*)malloc(sizeof(char*) * fragmentFileSize);
  fread(fragmentFileBuffer, 1, fragmentFileSize, fragmentFile);
  fclose(fragmentFile);

  VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
  vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  vertexShaderModuleCreateInfo.codeSize = vertexFileSize;
  vertexShaderModuleCreateInfo.pCode = (uint32_t*)vertexFileBuffer;
  
  VkShaderModule vertexShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &vertexShaderModuleCreateInfo, NULL, &vertexShaderModule) == VK_SUCCESS) {
    printf("created vertex shader module\n");
  }

  VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
  fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  fragmentShaderModuleCreateInfo.codeSize = fragmentFileSize;
  fragmentShaderModuleCreateInfo.pCode = (uint32_t*)fragmentFileBuffer;

  VkShaderModule fragmentShaderModule;
  if (vkCreateShaderModule(app->logicalDevice, &fragmentShaderModuleCreateInfo, NULL, &fragmentShaderModule) == VK_SUCCESS) {
    printf("created fragment shader module\n");
  }
 
  VkPipelineShaderStageCreateInfo vertexShaderStageInfo = {};
  vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertexShaderStageInfo.module = vertexShaderModule;
  vertexShaderStageInfo.pName = "main";
  
  VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = {};
  fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragmentShaderStageInfo.module = fragmentShaderModule;
  fragmentShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[2] = {vertexShaderStageInfo, fragmentShaderStageInfo};

  app->vertexBindingDescriptions = (VkVertexInputBindingDescription*)malloc(sizeof(VkVertexInputBindingDescription) * 2);
  app->vertexBindingDescriptions[0].binding = 0;
  app->vertexBindingDescriptions[0].stride = sizeof(float) * 2;
  app->vertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  app->vertexBindingDescriptions[1].binding = 1;
  app->vertexBindingDescriptions[1].stride = sizeof(float) * 3;
  app->vertexBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  app->vertexAttributeDescriptions = (VkVertexInputAttributeDescription*)malloc(sizeof(VkVertexInputAttributeDescription) * 2);
  app->vertexAttributeDescriptions[0].binding = 0;
  app->vertexAttributeDescriptions[0].location = 0;
  app->vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
  app->vertexAttributeDescriptions[0].offset = 0;
  
  app->vertexAttributeDescriptions[1].binding = 1;
  app->vertexAttributeDescriptions[1].location = 1;
  app->vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  app->vertexAttributeDescriptions[1].offset = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
  vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputStateCreateInfo.vertexBindingDescriptionCount = 2;
  vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
  vertexInputStateCreateInfo.pVertexBindingDescriptions = app->vertexBindingDescriptions;
  vertexInputStateCreateInfo.pVertexAttributeDescriptions = app->vertexAttributeDescriptions;
  
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
  inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
 
  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)app->swapchainExtent.width;
  viewport.height = (float)app->swapchainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  VkOffset2D scissorOffset = {0, 0};
  scissor.offset = scissorOffset;
  scissor.extent = app->swapchainExtent;

  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
  viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateCreateInfo.viewportCount = 1;
  viewportStateCreateInfo.pViewports = &viewport;
  viewportStateCreateInfo.scissorCount = 1;
  viewportStateCreateInfo.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
  rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
  rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationStateCreateInfo.lineWidth = 1.0f;
  rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
  multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
  multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
  colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachmentState.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};  
  colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
  colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
  colorBlendStateCreateInfo.attachmentCount = 1;
  colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
  colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
  colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
  colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
  colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts = &app->descriptorSetLayout;

  if (vkCreatePipelineLayout(app->logicalDevice, &pipelineLayoutCreateInfo, NULL, &app->pipelineLayout) == VK_SUCCESS) {
    printf("created pipeline layout\n");
  } 

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
  graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphicsPipelineCreateInfo.stageCount = 2;
  graphicsPipelineCreateInfo.pStages = shaderStages;
  graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
  graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
  graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
  graphicsPipelineCreateInfo.layout = app->pipelineLayout;
  graphicsPipelineCreateInfo.renderPass = app->renderPass;
  graphicsPipelineCreateInfo.subpass = 0;
  graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;  

  if (vkCreateGraphicsPipelines(app->logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, NULL, &app->graphicsPipeline) == VK_SUCCESS) {
    printf("created graphics pipeline\n");
  }
}

void createFramebuffers(struct VulkanApplication* app) {
  app->swapchainFramebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer*) * app->imageCount);
  
  for (int x = 0; x < app->imageCount; x++) {
    VkImageView attachments[1] = {
      app->swapchainImageViews[x]
    };

    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = app->renderPass;
    framebufferCreateInfo.attachmentCount = 1;
    framebufferCreateInfo.pAttachments = attachments;
    framebufferCreateInfo.width = app->swapchainExtent.width;
    framebufferCreateInfo.height = app->swapchainExtent.height;
    framebufferCreateInfo.layers = 1;

    if (vkCreateFramebuffer(app->logicalDevice, &framebufferCreateInfo, NULL, &app->swapchainFramebuffers[x]) == VK_SUCCESS) {
      printf("created swapchain framebuffer #%d\n", x);
    }
  }
}

void createCommandPool(struct VulkanApplication* app) {
  VkCommandPoolCreateInfo commandPoolCreateInfo = {};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = app->graphicsQueueIndex;

  if (vkCreateCommandPool(app->logicalDevice, &commandPoolCreateInfo, NULL, &app->commandPool) == VK_SUCCESS) {
    printf("created command pool\n");
  }
}

void createBuffer(struct VulkanApplication* app, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = usageFlags;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(app->logicalDevice, &bufferCreateInfo, NULL, buffer) == VK_SUCCESS) {
    printf("created buffer\n");
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

void createVertexBuffer(struct VulkanApplication* app) {
  VkDeviceSize positionBufferSize = sizeof(float) * 8;
  
  VkBuffer positionStagingBuffer;
  VkDeviceMemory positionStagingBufferMemory;
  createBuffer(app, positionBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &positionStagingBuffer, &positionStagingBufferMemory);

  void* positionData;
  vkMapMemory(app->logicalDevice, positionStagingBufferMemory, 0, positionBufferSize, 0, &positionData);
  memcpy(positionData, verticesPosition, positionBufferSize);
  vkUnmapMemory(app->logicalDevice, positionStagingBufferMemory);

  createBuffer(app, positionBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->vertexPositionBuffer, &app->vertexPositionBufferMemory);  

  copyBuffer(app, positionStagingBuffer, app->vertexPositionBuffer, positionBufferSize);

  vkDestroyBuffer(app->logicalDevice, positionStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, positionStagingBufferMemory, NULL);

  VkDeviceSize colorBufferSize = sizeof(float) * 12;
  
  VkBuffer colorStagingBuffer;
  VkDeviceMemory colorStagingBufferMemory;
  createBuffer(app, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &colorStagingBuffer, &colorStagingBufferMemory);

  void* colorData;
  vkMapMemory(app->logicalDevice, colorStagingBufferMemory, 0, colorBufferSize, 0, &colorData);
  memcpy(colorData, verticesColor, colorBufferSize);
  vkUnmapMemory(app->logicalDevice, colorStagingBufferMemory);

  createBuffer(app, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->vertexColorBuffer, &app->vertexColorBufferMemory);  

  copyBuffer(app, colorStagingBuffer, app->vertexColorBuffer, colorBufferSize);

  vkDestroyBuffer(app->logicalDevice, colorStagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, colorStagingBufferMemory, NULL);
}

void createIndexBuffer(struct VulkanApplication* app) {
  VkDeviceSize bufferSize = sizeof(uint32_t) * 6;
  
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  void* data;
  vkMapMemory(app->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertexIndices, bufferSize);
  vkUnmapMemory(app->logicalDevice, stagingBufferMemory);

  createBuffer(app, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &app->indexBuffer, &app->indexBufferMemory);

  copyBuffer(app, stagingBuffer, app->indexBuffer, bufferSize);
  
  vkDestroyBuffer(app->logicalDevice, stagingBuffer, NULL);
  vkFreeMemory(app->logicalDevice, stagingBufferMemory, NULL);
}

void createUniformBuffers(struct VulkanApplication* app) {
  VkDeviceSize bufferSize = sizeof(struct UniformBufferObject);

  app->uniformBuffers = (VkBuffer*)malloc(sizeof(VkBuffer) * app->imageCount);
  app->uniformBuffersMemory = (VkDeviceMemory*)malloc(sizeof(VkDeviceMemory) * app->imageCount);

  for (int x = 0; x < app->imageCount; x++) {
    createBuffer(app, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &app->uniformBuffers[x], &app->uniformBuffersMemory[x]);
  }
}

void createDescriptorPool(struct VulkanApplication* app) {
  VkDescriptorPoolSize descriptorPoolSize = {};
  descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorPoolSize.descriptorCount = app->imageCount;

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.poolSizeCount = 1;
  descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
  descriptorPoolCreateInfo.maxSets = app->imageCount;

  if (vkCreateDescriptorPool(app->logicalDevice, &descriptorPoolCreateInfo, NULL, &app->descriptorPool) == VK_SUCCESS) {
    printf("created descriptor pool\n");
  }
}

void createDescriptorSets(struct VulkanApplication* app) {
  VkDescriptorSetLayout* descriptorSetLayouts = (VkDescriptorSetLayout*)malloc(sizeof(VkDescriptorSetLayout) * app->imageCount);
  for (int x = 0; x < app->imageCount; x++) {
    descriptorSetLayouts[x] = app->descriptorSetLayout;
  }

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool = app->descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = app->imageCount;
  descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts;

  app->descriptorSets = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * app->imageCount);
  if (vkAllocateDescriptorSets(app->logicalDevice, &descriptorSetAllocateInfo, app->descriptorSets) == VK_SUCCESS) {
    printf("allocated descriptor sets\n");
  }

  for (int x = 0; x < app->imageCount; x++) {
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = app->uniformBuffers[x];
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(struct UniformBufferObject);

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = app->descriptorSets[x];
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
  
    vkUpdateDescriptorSets(app->logicalDevice, 1, &writeDescriptorSet, 0, NULL);
  }
}

void createCommandBuffers(struct VulkanApplication* app) {
  app->commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * app->imageCount);
  
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = app->commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = app->imageCount;

  if (vkAllocateCommandBuffers(app->logicalDevice, &commandBufferAllocateInfo, app->commandBuffers) == VK_SUCCESS) {
    printf("allocated command buffers\n");
  }

  for (int x = 0; x < app->imageCount; x++) {
    VkCommandBufferBeginInfo commandBufferBeginCreateInfo = {};
    commandBufferBeginCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(app->commandBuffers[x], &commandBufferBeginCreateInfo) == VK_SUCCESS) {
      printf("begin recording command buffer for image #%d\n", x);
    }
      
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = app->renderPass;
    renderPassBeginInfo.framebuffer = app->swapchainFramebuffers[x];
    VkOffset2D renderAreaOffset = {0, 0};
    renderPassBeginInfo.renderArea.offset = renderAreaOffset;
    renderPassBeginInfo.renderArea.extent = app->swapchainExtent;

    VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(app->commandBuffers[x], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(app->commandBuffers[x], VK_PIPELINE_BIND_POINT_GRAPHICS, app->graphicsPipeline);

    VkBuffer vertexBuffers[2] = {app->vertexPositionBuffer, app->vertexColorBuffer};
    VkDeviceSize offsets[2] = {0, 0};
    vkCmdBindVertexBuffers(app->commandBuffers[x], 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(app->commandBuffers[x], app->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(app->commandBuffers[x], VK_PIPELINE_BIND_POINT_GRAPHICS, app->pipelineLayout, 0, 1, &app->descriptorSets[x], 0, NULL);    
    
    vkCmdDrawIndexed(app->commandBuffers[x], 6, 1, 0, 0, 0);
    vkCmdEndRenderPass(app->commandBuffers[x]);

    if (vkEndCommandBuffer(app->commandBuffers[x]) == VK_SUCCESS) {
      printf("end recording command buffer for image #%d\n", x);
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

void updateUniformBuffer(struct VulkanApplication* app, uint32_t currentImage) {
  struct UniformBufferObject ubo = {};
  ubo.uniformTest = 1.0f;

  void* data;
  vkMapMemory(app->logicalDevice, app->uniformBuffersMemory[currentImage], 0, sizeof(struct UniformBufferObject), 0, &data);
  memcpy(data, &ubo, sizeof(struct UniformBufferObject));
  vkUnmapMemory(app->logicalDevice, app->uniformBuffersMemory[currentImage]);
}

void drawFrame(struct VulkanApplication* app) {
  vkWaitForFences(app->logicalDevice, 1, &app->inFlightFences[app->currentFrame], VK_TRUE, UINT64_MAX);
    
  uint32_t imageIndex;
  vkAcquireNextImageKHR(app->logicalDevice, app->swapchain, UINT64_MAX, app->imageAvailableSemaphores[app->currentFrame], VK_NULL_HANDLE, &imageIndex);
    
  if (app->imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(app->logicalDevice, 1, &app->imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
  }
  app->imagesInFlight[imageIndex] = app->inFlightFences[app->currentFrame];
 
  updateUniformBuffer(app, imageIndex);
   
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

void runMainLoop(struct VulkanApplication* app) {
  while (!glfwWindowShouldClose(app->window)) {
    glfwPollEvents();
    
    drawFrame(app);
  }
}

int main(void) {
  struct VulkanApplication* app = (struct VulkanApplication*)malloc(sizeof(struct VulkanApplication));
  initializeVulkanContext(app);
  pickPhysicalDevice(app);
  createLogicalConnection(app);
  createSwapchain(app);  
  createRenderPass(app);
  createDescriptorSetLayout(app);
  createGraphicsPipeline(app);
  createFramebuffers(app);
  createCommandPool(app);
  createVertexBuffer(app);
  createIndexBuffer(app);
  createUniformBuffers(app);
  createDescriptorPool(app);
  createDescriptorSets(app);
  createCommandBuffers(app);
  createSynchronizationObjects(app);

  runMainLoop(app);
  
  return 0;
}
