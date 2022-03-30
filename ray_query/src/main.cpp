#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <fstream>
#include <iostream>
#include <vector>

#define STRING_RESET "\033[0m"
#define STRING_INFO "\033[37m"
#define STRING_WARNING "\033[33m"
#define STRING_ERROR "\033[36m"

#define PRINT_MESSAGE(stream, message) stream << message << std::endl;

void keyCallback(GLFWwindow *windowPtr, int key, int scancode, int action,
                 int mods) {}

VkBool32
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {

  std::string message = pCallbackData->pMessage;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    message = STRING_INFO + message + STRING_RESET;
    PRINT_MESSAGE(std::cout, message.c_str());
  }

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    message = STRING_WARNING + message + STRING_RESET;
    PRINT_MESSAGE(std::cerr, message.c_str());
  }

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    message = STRING_ERROR + message + STRING_RESET;
    PRINT_MESSAGE(std::cerr, message.c_str());
  }

  return VK_FALSE;
}

void throwExceptionVulkanAPI(VkResult result, std::string functionName) {
  std::string message = "Vulkan API exception: return code " +
                        std::to_string(result) + " (" + functionName + ")";

  throw std::runtime_error(message);
}

void throwExceptionMessage(std::string message) {
  throw std::runtime_error(message);
}

int main() {
  VkResult result;

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow *windowPtr = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
  glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(windowPtr, keyCallback);

  std::vector<VkValidationFeatureEnableEXT> validationFeatureEnableList = {
      // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

  VkDebugUtilsMessageSeverityFlagBitsEXT debugUtilsMessageSeverityFlagBits =
      (VkDebugUtilsMessageSeverityFlagBitsEXT)(
          // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);

  VkDebugUtilsMessageTypeFlagBitsEXT debugUtilsMessageTypeFlagBits =
      (VkDebugUtilsMessageTypeFlagBitsEXT)(
          // VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);

  VkValidationFeaturesEXT validationFeatures = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = NULL,
      .enabledValidationFeatureCount =
          (uint32_t)validationFeatureEnableList.size(),
      .pEnabledValidationFeatures = validationFeatureEnableList.data(),
      .disabledValidationFeatureCount = 0,
      .pDisabledValidationFeatures = NULL};

  VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .pNext = &validationFeatures,
      .flags = 0,
      .messageSeverity = debugUtilsMessageSeverityFlagBits,
      .messageType = debugUtilsMessageTypeFlagBits,
      .pfnUserCallback = &debugCallback,
      .pUserData = NULL};

  VkApplicationInfo applicationInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = NULL,
      .pApplicationName = "Ray Query Example",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3};

  std::vector<const char *> instanceLayerList = {"VK_LAYER_KHRONOS_validation"};

  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> instanceExtensionList(
      glfwExtensions, glfwExtensions + glfwExtensionCount);

  instanceExtensionList.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  instanceExtensionList.push_back("VK_KHR_get_physical_device_properties2");
  instanceExtensionList.push_back("VK_KHR_surface");

  VkInstanceCreateInfo instanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = &debugUtilsMessengerCreateInfo,
      .flags = 0,
      .pApplicationInfo = &applicationInfo,
      .enabledLayerCount = (uint32_t)instanceLayerList.size(),
      .ppEnabledLayerNames = instanceLayerList.data(),
      .enabledExtensionCount = (uint32_t)instanceExtensionList.size(),
      .ppEnabledExtensionNames = instanceExtensionList.data(),
  };

  VkInstance instanceHandle = VK_NULL_HANDLE;
  result = vkCreateInstance(&instanceCreateInfo, NULL, &instanceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateInstance");
  }

  VkSurfaceKHR surfaceHandle = VK_NULL_HANDLE;
  result =
      glfwCreateWindowSurface(instanceHandle, windowPtr, NULL, &surfaceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "glfwCreateWindowSurface");
  }

  uint32_t physicalDeviceCount = 0;
  result =
      vkEnumeratePhysicalDevices(instanceHandle, &physicalDeviceCount, NULL);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEnumeratePhysicalDevices");
  }

  std::vector<VkPhysicalDevice> physicalDeviceHandleList(physicalDeviceCount);
  result = vkEnumeratePhysicalDevices(instanceHandle, &physicalDeviceCount,
                                      physicalDeviceHandleList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEnumeratePhysicalDevices");
  }

  VkPhysicalDevice activePhysicalDeviceHandle = physicalDeviceHandleList[0];

  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(activePhysicalDeviceHandle,
                                &physicalDeviceProperties);

  VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
  vkGetPhysicalDeviceMemoryProperties(activePhysicalDeviceHandle,
                                      &physicalDeviceMemoryProperties);

  std::cout << physicalDeviceProperties.deviceName << std::endl;

  VkPhysicalDeviceBufferDeviceAddressFeatures
      physicalDeviceBufferDeviceAddressFeatures = {
          .sType =
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
          .pNext = NULL,
          .bufferDeviceAddress = VK_TRUE,
          .bufferDeviceAddressCaptureReplay = VK_FALSE,
          .bufferDeviceAddressMultiDevice = VK_FALSE};

  VkPhysicalDeviceAccelerationStructureFeaturesKHR
      physicalDeviceAccelerationStructureFeatures = {
          .sType =
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
          .pNext = &physicalDeviceBufferDeviceAddressFeatures,
          .accelerationStructure = VK_TRUE,
          .accelerationStructureCaptureReplay = VK_FALSE,
          .accelerationStructureIndirectBuild = VK_FALSE,
          .accelerationStructureHostCommands = VK_FALSE,
          .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE};

  VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeatures = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
      .pNext = &physicalDeviceAccelerationStructureFeatures,
      .rayQuery = VK_TRUE};

  VkPhysicalDeviceFeatures deviceFeatures = {.geometryShader = VK_TRUE};

  uint32_t queueFamilyPropertyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(activePhysicalDeviceHandle,
                                           &queueFamilyPropertyCount, NULL);

  std::vector<VkQueueFamilyProperties> queueFamilyPropertiesList(
      queueFamilyPropertyCount);

  vkGetPhysicalDeviceQueueFamilyProperties(activePhysicalDeviceHandle,
                                           &queueFamilyPropertyCount,
                                           queueFamilyPropertiesList.data());

  uint32_t queueFamilyIndex = -1;
  for (uint32_t x = 0; x < queueFamilyPropertiesList.size(); x++) {
    if (queueFamilyPropertiesList[x].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

      VkBool32 isPresentSupported = false;
      result = vkGetPhysicalDeviceSurfaceSupportKHR(
          activePhysicalDeviceHandle, x, surfaceHandle, &isPresentSupported);

      if (result != VK_SUCCESS) {
        throwExceptionVulkanAPI(result, "vkGetPhysicalDeviceSurfaceSupportKHR");
      }

      if (isPresentSupported) {
        queueFamilyIndex = x;
        break;
      }
    }
  }

  std::vector<float> queuePrioritiesList = {1.0f};
  VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .queueFamilyIndex = queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = queuePrioritiesList.data()};

  std::vector<const char *> deviceExtensionList = {
      "VK_KHR_ray_query",
      "VK_KHR_spirv_1_4",
      "VK_KHR_shader_float_controls",
      "VK_KHR_acceleration_structure",
      "VK_EXT_descriptor_indexing",
      "VK_KHR_maintenance3",
      "VK_KHR_buffer_device_address",
      "VK_KHR_deferred_host_operations",
      "VK_KHR_swapchain"};

  VkDeviceCreateInfo deviceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &physicalDeviceRayQueryFeatures,
      .flags = 0,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = NULL,
      .enabledExtensionCount = (uint32_t)deviceExtensionList.size(),
      .ppEnabledExtensionNames = deviceExtensionList.data(),
      .pEnabledFeatures = &deviceFeatures};

  VkDevice deviceHandle = VK_NULL_HANDLE;
  result = vkCreateDevice(activePhysicalDeviceHandle, &deviceCreateInfo, NULL,
                          &deviceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateDevice");
  }

  VkQueue queueHandle = VK_NULL_HANDLE;
  vkGetDeviceQueue(deviceHandle, queueFamilyIndex, 0, &queueHandle);

  PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR =
      (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(
          deviceHandle, "vkGetBufferDeviceAddressKHR");

  PFN_vkGetAccelerationStructureBuildSizesKHR
      pvkGetAccelerationStructureBuildSizesKHR =
          (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(
              deviceHandle, "vkGetAccelerationStructureBuildSizesKHR");

  PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR =
      (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(
          deviceHandle, "vkCreateAccelerationStructureKHR");

  PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR =
      (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(
          deviceHandle, "vkCmdBuildAccelerationStructuresKHR");

  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .pNext = NULL,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
      .deviceMask = 0};

  VkCommandPoolCreateInfo commandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex};

  VkCommandPool commandPoolHandle = VK_NULL_HANDLE;
  result = vkCreateCommandPool(deviceHandle, &commandPoolCreateInfo, NULL,
                               &commandPoolHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateCommandPool");
  }

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = NULL,
      .commandPool = commandPoolHandle,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 16};

  std::vector<VkCommandBuffer> commandBufferHandleList =
      std::vector<VkCommandBuffer>(16, VK_NULL_HANDLE);

  result = vkAllocateCommandBuffers(deviceHandle, &commandBufferAllocateInfo,
                                    commandBufferHandleList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateCommandBuffers");
  }

  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      activePhysicalDeviceHandle, surfaceHandle, &surfaceCapabilities);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result,
                            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  }

  uint32_t surfaceFormatCount = 0;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      activePhysicalDeviceHandle, surfaceHandle, &surfaceFormatCount, NULL);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
  }

  std::vector<VkSurfaceFormatKHR> surfaceFormatList(surfaceFormatCount);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      activePhysicalDeviceHandle, surfaceHandle, &surfaceFormatCount,
      surfaceFormatList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkGetPhysicalDeviceSurfaceFormatsKHR");
  }

  uint32_t presentModeCount = 0;
  result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      activePhysicalDeviceHandle, surfaceHandle, &presentModeCount, NULL);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result,
                            "vkGetPhysicalDeviceSurfacePresentModesKHR");
  }

  std::vector<VkPresentModeKHR> presentModeList(presentModeCount);
  result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      activePhysicalDeviceHandle, surfaceHandle, &presentModeCount,
      presentModeList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result,
                            "vkGetPhysicalDeviceSurfacePresentModesKHR");
  }

  VkSwapchainCreateInfoKHR swapchainCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .surface = surfaceHandle,
      .minImageCount = surfaceCapabilities.minImageCount + 1,
      .imageFormat = surfaceFormatList[0].format,
      .imageColorSpace = surfaceFormatList[0].colorSpace,
      .imageExtent = surfaceCapabilities.currentExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = presentModeList[0],
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE};

  VkSwapchainKHR swapchainHandle = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(deviceHandle, &swapchainCreateInfo, NULL,
                                &swapchainHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateSwapchainKHR");
  }

  std::vector<VkAttachmentDescription> attachmentDescriptionList = {
      {.flags = 0,
       .format = surfaceFormatList[0].format,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}};

  std::vector<VkAttachmentReference> attachmentReferenceList = {
      {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpassDescription = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .colorAttachmentCount = (uint32_t)attachmentReferenceList.size(),
      .pColorAttachments = attachmentReferenceList.data(),
      .pResolveAttachments = NULL,
      .pDepthStencilAttachment = NULL,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL};

  VkRenderPassCreateInfo renderPassCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .attachmentCount = (uint32_t)attachmentDescriptionList.size(),
      .pAttachments = attachmentDescriptionList.data(),
      .subpassCount = 1,
      .pSubpasses = &subpassDescription,
      .dependencyCount = 0,
      .pDependencies = NULL};

  VkRenderPass renderPassHandle = VK_NULL_HANDLE;
  result = vkCreateRenderPass(deviceHandle, &renderPassCreateInfo, NULL,
                              &renderPassHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateRenderPass");
  }

  uint32_t swapchainImageCount = 0;
  result = vkGetSwapchainImagesKHR(deviceHandle, swapchainHandle,
                                   &swapchainImageCount, NULL);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkGetSwapchainImagesKHR");
  }

  std::vector<VkImage> swapchainImageHandleList(swapchainImageCount);
  result = vkGetSwapchainImagesKHR(deviceHandle, swapchainHandle,
                                   &swapchainImageCount,
                                   swapchainImageHandleList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkGetSwapchainImagesKHR");
  }

  std::vector<VkImageView> swapchainImageViewHandleList(swapchainImageCount,
                                                        VK_NULL_HANDLE);
  std::vector<VkFramebuffer> framebufferHandleList(swapchainImageCount,
                                                   VK_NULL_HANDLE);

  for (uint32_t x = 0; x < swapchainImageCount; x++) {
    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = swapchainImageHandleList[x],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surfaceFormatList[0].format,
        .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    result = vkCreateImageView(deviceHandle, &imageViewCreateInfo, NULL,
                               &swapchainImageViewHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateImageView");
    }

    VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPassHandle,
        .attachmentCount = 1,
        .pAttachments = &swapchainImageViewHandleList[x],
        .width = surfaceCapabilities.currentExtent.width,
        .height = surfaceCapabilities.currentExtent.height,
        .layers = 1};

    result = vkCreateFramebuffer(deviceHandle, &framebufferCreateInfo, NULL,
                                 &framebufferHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateFramebuffer");
    }
  }

  std::vector<VkDescriptorPoolSize> descriptorPoolSizeList = {
      {.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
       .descriptorCount = 1},
      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 4},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1}};

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 2,
      .poolSizeCount = (uint32_t)descriptorPoolSizeList.size(),
      .pPoolSizes = descriptorPoolSizeList.data()};

  VkDescriptorPool descriptorPoolHandle = VK_NULL_HANDLE;
  result = vkCreateDescriptorPool(deviceHandle, &descriptorPoolCreateInfo, NULL,
                                  &descriptorPoolHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateDescriptorPool");
  }

  std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindingList = {
      {.binding = 0,
       .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
       .pImmutableSamplers = NULL},
      {.binding = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
       .pImmutableSamplers = NULL},
      {.binding = 2,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
       .pImmutableSamplers = NULL},
      {.binding = 3,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
       .pImmutableSamplers = NULL},
      {.binding = 4,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       .descriptorCount = 1,
       .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
       .pImmutableSamplers = NULL}};

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .bindingCount = (uint32_t)descriptorSetLayoutBindingList.size(),
      .pBindings = descriptorSetLayoutBindingList.data()};

  VkDescriptorSetLayout descriptorSetLayoutHandle = VK_NULL_HANDLE;
  result =
      vkCreateDescriptorSetLayout(deviceHandle, &descriptorSetLayoutCreateInfo,
                                  NULL, &descriptorSetLayoutHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateDescriptorSetLayout");
  }

  std::vector<VkDescriptorSetLayoutBinding>
      materialDescriptorSetLayoutBindingList = {
          {.binding = 0,
           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
           .descriptorCount = 1,
           .stageFlags =
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
           .pImmutableSamplers = NULL},
          {.binding = 1,
           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
           .descriptorCount = 1,
           .stageFlags =
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
           .pImmutableSamplers = NULL}};

  VkDescriptorSetLayoutCreateInfo materialDescriptorSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .bindingCount = (uint32_t)materialDescriptorSetLayoutBindingList.size(),
      .pBindings = materialDescriptorSetLayoutBindingList.data()};

  VkDescriptorSetLayout materialDescriptorSetLayoutHandle = VK_NULL_HANDLE;
  result = vkCreateDescriptorSetLayout(
      deviceHandle, &materialDescriptorSetLayoutCreateInfo, NULL,
      &materialDescriptorSetLayoutHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateDescriptorSetLayout");
  }

  std::vector<VkDescriptorSetLayout> descriptorSetLayoutHandleList = {
      descriptorSetLayoutHandle, materialDescriptorSetLayoutHandle};

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = NULL,
      .descriptorPool = descriptorPoolHandle,
      .descriptorSetCount = (uint32_t)descriptorSetLayoutHandleList.size(),
      .pSetLayouts = descriptorSetLayoutHandleList.data()};

  std::vector<VkDescriptorSet> descriptorSetHandleList =
      std::vector<VkDescriptorSet>(2, VK_NULL_HANDLE);

  result = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocateInfo,
                                    descriptorSetHandleList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateDescriptorSets");
  }

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .setLayoutCount = (uint32_t)descriptorSetLayoutHandleList.size(),
      .pSetLayouts = descriptorSetLayoutHandleList.data(),
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = NULL};

  VkPipelineLayout pipelineLayoutHandle = VK_NULL_HANDLE;
  result = vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCreateInfo, NULL,
                                  &pipelineLayoutHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreatePipelineLayout");
  }

  std::ifstream vertexFile("shaders/basic.vert.spv",
                           std::ios::binary | std::ios::ate);
  std::streamsize vertexFileSize = vertexFile.tellg();
  vertexFile.seekg(0, std::ios::beg);
  std::vector<uint32_t> vertexShaderSource(vertexFileSize / sizeof(uint32_t));
  vertexFile.read((char *)vertexShaderSource.data(), vertexFileSize);
  vertexFile.close();

  VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .codeSize = (uint32_t)vertexShaderSource.size() * sizeof(uint32_t),
      .pCode = vertexShaderSource.data()};

  VkShaderModule vertexShaderModuleHandle = VK_NULL_HANDLE;
  result = vkCreateShaderModule(deviceHandle, &vertexShaderModuleCreateInfo,
                                NULL, &vertexShaderModuleHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateShaderModule");
  }

  std::ifstream fragmentFile("shaders/basic.frag.spv",
                             std::ios::binary | std::ios::ate);
  std::streamsize fragmentFileSize = fragmentFile.tellg();
  fragmentFile.seekg(0, std::ios::beg);
  std::vector<uint32_t> fragmentShaderSource(fragmentFileSize /
                                             sizeof(uint32_t));
  fragmentFile.read((char *)fragmentShaderSource.data(), fragmentFileSize);
  fragmentFile.close();

  VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .codeSize = (uint32_t)fragmentShaderSource.size() * sizeof(uint32_t),
      .pCode = fragmentShaderSource.data()};

  VkShaderModule fragmentShaderModuleHandle = VK_NULL_HANDLE;
  result = vkCreateShaderModule(deviceHandle, &fragmentShaderModuleCreateInfo,
                                NULL, &fragmentShaderModuleHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateShaderModule");
  }

  std::vector<VkPipelineShaderStageCreateInfo>
      pipelineShaderStageCreateInfoList = {
          {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
           .pNext = NULL,
           .flags = 0,
           .stage = VK_SHADER_STAGE_VERTEX_BIT,
           .module = vertexShaderModuleHandle,
           .pName = "main",
           .pSpecializationInfo = NULL},
          {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
           .pNext = NULL,
           .flags = 0,
           .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
           .module = fragmentShaderModuleHandle,
           .pName = "main",
           .pSpecializationInfo = NULL}};

  VkVertexInputBindingDescription vertexInputBindingDescription = {
      .binding = 0,
      .stride = sizeof(float) * 3,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

  VkVertexInputAttributeDescription vertexInputAttributeDescription = {
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = 0};

  VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertexInputBindingDescription,
      .vertexAttributeDescriptionCount = 1,
      .pVertexAttributeDescriptions = &vertexInputAttributeDescription};

  VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo =
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
       .pNext = NULL,
       .flags = 0,
       .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
       .primitiveRestartEnable = VK_FALSE};

  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)surfaceCapabilities.currentExtent.width,
      .height = (float)surfaceCapabilities.currentExtent.height,
      .minDepth = 0,
      .maxDepth = 1};

  VkRect2D rect2D = {.offset = {.x = 0, .y = 0},
                     .extent = surfaceCapabilities.currentExtent};

  VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &rect2D};

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo =
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
       .pNext = NULL,
       .flags = 0,
       .depthClampEnable = VK_FALSE,
       .rasterizerDiscardEnable = VK_FALSE,
       .polygonMode = VK_POLYGON_MODE_FILL,
       .cullMode = VK_CULL_MODE_NONE,
       .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
       .depthBiasEnable = VK_FALSE,
       .depthBiasConstantFactor = 0.0,
       .depthBiasClamp = 0.0,
       .depthBiasSlopeFactor = 0.0,
       .lineWidth = 1.0};

  VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 0.0,
      .pSampleMask = NULL,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &pipelineColorBlendAttachmentState,
      .blendConstants = {0, 0, 0, 0}};

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stageCount = (uint32_t)pipelineShaderStageCreateInfoList.size(),
      .pStages = pipelineShaderStageCreateInfoList.data(),
      .pVertexInputState = &pipelineVertexInputStateCreateInfo,
      .pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo,
      .pTessellationState = NULL,
      .pViewportState = &pipelineViewportStateCreateInfo,
      .pRasterizationState = &pipelineRasterizationStateCreateInfo,
      .pMultisampleState = &pipelineMultisampleStateCreateInfo,
      .pDepthStencilState = NULL,
      .pColorBlendState = &pipelineColorBlendStateCreateInfo,
      .pDynamicState = NULL,
      .layout = pipelineLayoutHandle,
      .renderPass = renderPassHandle,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = 0};

  VkPipeline graphicsPipelineHandle = VK_NULL_HANDLE;
  result = vkCreateGraphicsPipelines(deviceHandle, VK_NULL_HANDLE, 1,
                                     &graphicsPipelineCreateInfo, NULL,
                                     &graphicsPipelineHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateGraphicsPipelines");
  }

  tinyobj::ObjReaderConfig reader_config;
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile("resources/cube_scene.obj", reader_config)) {
    if (!reader.Error().empty()) {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(1);
  }

  if (!reader.Warning().empty()) {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

  const tinyobj::attrib_t &attrib = reader.GetAttrib();
  const std::vector<tinyobj::shape_t> &shapes = reader.GetShapes();
  const std::vector<tinyobj::material_t> &materials = reader.GetMaterials();

  uint32_t primitiveCount = 0;
  std::vector<uint32_t> indexList;
  for (tinyobj::shape_t shape : shapes) {

    primitiveCount += shape.mesh.num_face_vertices.size();

    for (tinyobj::index_t index : shape.mesh.indices) {
      indexList.push_back(index.vertex_index);
    }
  }

  VkBufferCreateInfo vertexBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(float) * attrib.vertices.size() * 3,
      .usage =
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer vertexBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle, &vertexBufferCreateInfo, NULL,
                          &vertexBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements vertexMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle, vertexBufferHandle,
                                &vertexMemoryRequirements);

  uint32_t vertexMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((vertexMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      vertexMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo vertexMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = vertexMemoryRequirements.size,
      .memoryTypeIndex = vertexMemoryTypeIndex};

  VkDeviceMemory vertexDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &vertexMemoryAllocateInfo, NULL,
                            &vertexDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(deviceHandle, vertexBufferHandle,
                              vertexDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostVertexMemoryBuffer;
  result = vkMapMemory(deviceHandle, vertexDeviceMemoryHandle, 0,
                       sizeof(float) * attrib.vertices.size() * 3, 0,
                       &hostVertexMemoryBuffer);

  memcpy(hostVertexMemoryBuffer, attrib.vertices.data(),
         sizeof(float) * attrib.vertices.size() * 3);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, vertexDeviceMemoryHandle);

  VkBufferDeviceAddressInfo vertexBufferDeviceAddressInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .pNext = NULL,
      .buffer = vertexBufferHandle};

  VkDeviceAddress vertexBufferDeviceAddress = pvkGetBufferDeviceAddressKHR(
      deviceHandle, &vertexBufferDeviceAddressInfo);

  VkBufferCreateInfo indexBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(uint32_t) * indexList.size(),
      .usage =
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer indexBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle, &indexBufferCreateInfo, NULL,
                          &indexBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements indexMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle, indexBufferHandle,
                                &indexMemoryRequirements);

  uint32_t indexMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((indexMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      indexMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo indexMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = indexMemoryRequirements.size,
      .memoryTypeIndex = indexMemoryTypeIndex};

  VkDeviceMemory indexDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &indexMemoryAllocateInfo, NULL,
                            &indexDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(deviceHandle, indexBufferHandle,
                              indexDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostIndexMemoryBuffer;
  result = vkMapMemory(deviceHandle, indexDeviceMemoryHandle, 0,
                       sizeof(uint32_t) * indexList.size(), 0,
                       &hostIndexMemoryBuffer);

  memcpy(hostIndexMemoryBuffer, indexList.data(),
         sizeof(uint32_t) * indexList.size());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, indexDeviceMemoryHandle);

  VkBufferDeviceAddressInfo indexBufferDeviceAddressInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .pNext = NULL,
      .buffer = indexBufferHandle};

  VkDeviceAddress indexBufferDeviceAddress =
      pvkGetBufferDeviceAddressKHR(deviceHandle, &indexBufferDeviceAddressInfo);

  VkAccelerationStructureGeometryDataKHR
      bottomLevelAccelerationStructureGeometryData = {
          .triangles = {
              .sType =
                  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
              .pNext = NULL,
              .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
              .vertexData = {.deviceAddress = vertexBufferDeviceAddress},
              .vertexStride = sizeof(float) * 3,
              .maxVertex = (uint32_t)attrib.vertices.size(),
              .indexType = VK_INDEX_TYPE_UINT32,
              .indexData = {.deviceAddress = indexBufferDeviceAddress},
              .transformData = {.deviceAddress = 0}}};

  VkAccelerationStructureGeometryKHR bottomLevelAccelerationStructureGeometry =
      {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
       .pNext = NULL,
       .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
       .geometry = bottomLevelAccelerationStructureGeometryData,
       .flags = VK_GEOMETRY_OPAQUE_BIT_KHR};

  VkAccelerationStructureBuildGeometryInfoKHR
      bottomLevelAccelerationStructureBuildGeometryInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
          .pNext = NULL,
          .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
          .flags = 0,
          .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
          .srcAccelerationStructure = VK_NULL_HANDLE,
          .dstAccelerationStructure = VK_NULL_HANDLE,
          .geometryCount = 1,
          .pGeometries = &bottomLevelAccelerationStructureGeometry,
          .ppGeometries = NULL,
          .scratchData = {.deviceAddress = 0}};

  VkAccelerationStructureBuildSizesInfoKHR
      bottomLevelAccelerationStructureBuildSizesInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
          .pNext = NULL,
          .accelerationStructureSize = 0,
          .updateScratchSize = 0,
          .buildScratchSize = 0};

  std::vector<uint32_t> bottomLevelMaxPrimitiveCountList = {primitiveCount};

  pvkGetAccelerationStructureBuildSizesKHR(
      deviceHandle, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &bottomLevelAccelerationStructureBuildGeometryInfo,
      bottomLevelMaxPrimitiveCountList.data(),
      &bottomLevelAccelerationStructureBuildSizesInfo);

  VkBufferCreateInfo bottomLevelAccelerationStructureBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = bottomLevelAccelerationStructureBuildSizesInfo
                  .accelerationStructureSize,
      .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer bottomLevelAccelerationStructureBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle,
                          &bottomLevelAccelerationStructureBufferCreateInfo,
                          NULL, &bottomLevelAccelerationStructureBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements bottomLevelAccelerationStructureMemoryRequirements;
  vkGetBufferMemoryRequirements(
      deviceHandle, bottomLevelAccelerationStructureBufferHandle,
      &bottomLevelAccelerationStructureMemoryRequirements);

  uint32_t bottomLevelAccelerationStructureMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {

    if ((bottomLevelAccelerationStructureMemoryRequirements.memoryTypeBits &
         (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

      bottomLevelAccelerationStructureMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo bottomLevelAccelerationStructureMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,
      .allocationSize = bottomLevelAccelerationStructureMemoryRequirements.size,
      .memoryTypeIndex = bottomLevelAccelerationStructureMemoryTypeIndex};

  VkDeviceMemory bottomLevelAccelerationStructureDeviceMemoryHandle =
      VK_NULL_HANDLE;

  result = vkAllocateMemory(
      deviceHandle, &bottomLevelAccelerationStructureMemoryAllocateInfo, NULL,
      &bottomLevelAccelerationStructureDeviceMemoryHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(
      deviceHandle, bottomLevelAccelerationStructureBufferHandle,
      bottomLevelAccelerationStructureDeviceMemoryHandle, 0);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  VkAccelerationStructureCreateInfoKHR
      bottomLevelAccelerationStructureCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
          .pNext = NULL,
          .createFlags = 0,
          .buffer = bottomLevelAccelerationStructureBufferHandle,
          .offset = 0,
          .size = bottomLevelAccelerationStructureBuildSizesInfo
                      .accelerationStructureSize,
          .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
          .deviceAddress = 0};

  VkAccelerationStructureKHR bottomLevelAccelerationStructureHandle =
      VK_NULL_HANDLE;

  result = pvkCreateAccelerationStructureKHR(
      deviceHandle, &bottomLevelAccelerationStructureCreateInfo, NULL,
      &bottomLevelAccelerationStructureHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateAccelerationStructureKHR");
  }

  VkBufferCreateInfo bottomLevelAccelerationStructureScratchBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = bottomLevelAccelerationStructureBuildSizesInfo.buildScratchSize,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer bottomLevelAccelerationStructureScratchBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(
      deviceHandle, &bottomLevelAccelerationStructureScratchBufferCreateInfo,
      NULL, &bottomLevelAccelerationStructureScratchBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements
      bottomLevelAccelerationStructureScratchMemoryRequirements;
  vkGetBufferMemoryRequirements(
      deviceHandle, bottomLevelAccelerationStructureScratchBufferHandle,
      &bottomLevelAccelerationStructureScratchMemoryRequirements);

  uint32_t bottomLevelAccelerationStructureScratchMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {

    if ((bottomLevelAccelerationStructureScratchMemoryRequirements
             .memoryTypeBits &
         (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

      bottomLevelAccelerationStructureScratchMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo
      bottomLevelAccelerationStructureScratchMemoryAllocateInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .pNext = &memoryAllocateFlagsInfo,
          .allocationSize =
              bottomLevelAccelerationStructureScratchMemoryRequirements.size,
          .memoryTypeIndex =
              bottomLevelAccelerationStructureScratchMemoryTypeIndex};

  VkDeviceMemory bottomLevelAccelerationStructureDeviceScratchMemoryHandle =
      VK_NULL_HANDLE;

  result = vkAllocateMemory(
      deviceHandle, &bottomLevelAccelerationStructureScratchMemoryAllocateInfo,
      NULL, &bottomLevelAccelerationStructureDeviceScratchMemoryHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(
      deviceHandle, bottomLevelAccelerationStructureScratchBufferHandle,
      bottomLevelAccelerationStructureDeviceScratchMemoryHandle, 0);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  VkBufferDeviceAddressInfo
      bottomLevelAccelerationStructureScratchBufferDeviceAddressInfo = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          .pNext = NULL,
          .buffer = bottomLevelAccelerationStructureScratchBufferHandle};

  VkDeviceAddress bottomLevelAccelerationStructureScratchBufferDeviceAddress =
      pvkGetBufferDeviceAddressKHR(
          deviceHandle,
          &bottomLevelAccelerationStructureScratchBufferDeviceAddressInfo);

  bottomLevelAccelerationStructureBuildGeometryInfo.dstAccelerationStructure =
      bottomLevelAccelerationStructureHandle;

  bottomLevelAccelerationStructureBuildGeometryInfo.scratchData = {
      .deviceAddress =
          bottomLevelAccelerationStructureScratchBufferDeviceAddress};

  VkAccelerationStructureBuildRangeInfoKHR
      bottomLevelAccelerationStructureBuildRangeInfo = {.primitiveCount =
                                                            primitiveCount,
                                                        .primitiveOffset = 0,
                                                        .firstVertex = 0,
                                                        .transformOffset = 0};

  const VkAccelerationStructureBuildRangeInfoKHR
      *bottomLevelAccelerationStructureBuildRangeInfos =
          &bottomLevelAccelerationStructureBuildRangeInfo;

  VkCommandBufferBeginInfo commandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL};

  result = vkBeginCommandBuffer(commandBufferHandleList.back(),
                                &commandBufferBeginInfo);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBeginCommandBuffer");
  }

  pvkCmdBuildAccelerationStructuresKHR(
      commandBufferHandleList.back(), 1,
      &bottomLevelAccelerationStructureBuildGeometryInfo,
      &bottomLevelAccelerationStructureBuildRangeInfos);

  result = vkEndCommandBuffer(commandBufferHandleList.back());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEndCommandBuffer");
  }

  VkSubmitInfo bottomLevelAccelerationStructureBuildSubmitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = NULL,
      .pWaitDstStageMask = NULL,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferHandleList.back(),
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = NULL};

  VkFenceCreateInfo bottomLevelAccelerationStructureBuildFenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = 0};

  VkFence bottomLevelAccelerationStructureBuildFenceHandle = VK_NULL_HANDLE;
  result = vkCreateFence(
      deviceHandle, &bottomLevelAccelerationStructureBuildFenceCreateInfo, NULL,
      &bottomLevelAccelerationStructureBuildFenceHandle);

  result = vkQueueSubmit(queueHandle, 1,
                         &bottomLevelAccelerationStructureBuildSubmitInfo,
                         bottomLevelAccelerationStructureBuildFenceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkQueueSubmit");
  }

  result = vkWaitForFences(deviceHandle, 1,
                           &bottomLevelAccelerationStructureBuildFenceHandle,
                           true, UINT32_MAX);

  if (result != VK_SUCCESS && result != VK_TIMEOUT) {
    throwExceptionVulkanAPI(result, "vkWaitForFences");
  }

  return -1;
}
