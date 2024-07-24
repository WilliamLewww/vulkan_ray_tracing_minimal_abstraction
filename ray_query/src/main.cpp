#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <fstream>
#include <iostream>
#include <vector>

// REMOVE
static char keyDownIndex[500];

static float cameraPosition[3];
static float cameraYaw;
static float cameraPitch;

void keyCallback(GLFWwindow *windowPtr, int key, int scancode, int action,
                 int mods) {

  if (action == GLFW_PRESS) {
    keyDownIndex[key] = 1;
  }

  if (action == GLFW_RELEASE) {
    keyDownIndex[key] = 0;
  }
}
// REMOVE

#define M_PI 3.14159265358979323846264338327950288

#define PRINT_MESSAGE(stream, message) stream << message << std::endl;

#if defined(VALIDATION_ENABLED)
#define STRING_RESET "\033[0m"
#define STRING_INFO "\033[37m"
#define STRING_WARNING "\033[33m"
#define STRING_ERROR "\033[36m"

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
#endif

void throwExceptionVulkanAPI(VkResult result, const std::string& functionName) {
  std::string message = "Vulkan API exception: return code " +
                        std::to_string(result) + " (" + functionName + ")";

  throw std::runtime_error(message);
}

int main() {
  VkResult result;

  // =========================================================================
  // GLFW, Window

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow *windowPtr = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
  glfwSetInputMode(windowPtr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(windowPtr, keyCallback);

  // =========================================================================
  // Vulkan Instance

  VkDebugUtilsMessengerCreateInfoEXT *debugUtilsMessengerCreateInfoPtr = NULL;

#if defined(VALIDATION_ENABLED)
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
      .messageSeverity =
          (VkDebugUtilsMessageSeverityFlagsEXT)debugUtilsMessageSeverityFlagBits,
      .messageType =
          (VkDebugUtilsMessageTypeFlagsEXT)debugUtilsMessageTypeFlagBits,
      .pfnUserCallback = &debugCallback,
      .pUserData = NULL};

  debugUtilsMessengerCreateInfoPtr = &debugUtilsMessengerCreateInfo;
#endif

  VkApplicationInfo applicationInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = NULL,
      .pApplicationName = "Ray Query Example",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3};

  std::vector<const char *> instanceLayerList = {};
  std::vector<const char *> instanceExtensionList = {"VK_KHR_get_physical_device_properties2"};

  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> instanceExtensionList(
      glfwExtensions, glfwExtensions + glfwExtensionCount);

  instanceExtensionList.push_back("VK_KHR_get_physical_device_properties2");
  instanceExtensionList.push_back("VK_KHR_surface");

#if defined(VALIDATION_ENABLED)
  instanceLayerList.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensionList.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  VkInstanceCreateInfo instanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = debugUtilsMessengerCreateInfoPtr,
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

  // =========================================================================
  // Window Surface

  VkSurfaceKHR surfaceHandle = VK_NULL_HANDLE;
  result =
      glfwCreateWindowSurface(instanceHandle, windowPtr, NULL, &surfaceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "glfwCreateWindowSurface");
  }

  // =========================================================================
  // Physical Device

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

  // =========================================================================
  // Physical Device Features

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

  VkPhysicalDeviceFeatures deviceFeatures = {
      .geometryShader = VK_TRUE,
      .fragmentStoresAndAtomics = VK_TRUE};

  // =========================================================================
  // Physical Device Submission Queue Families

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

  // =========================================================================
  // Logical Device

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

  // =========================================================================
  // Submission Queue

  VkQueue queueHandle = VK_NULL_HANDLE;
  vkGetDeviceQueue(deviceHandle, queueFamilyIndex, 0, &queueHandle);

  // =========================================================================
  // Device Pointer Functions

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

  PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR =
      (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(
          deviceHandle, "vkDestroyAccelerationStructureKHR");

  PFN_vkGetAccelerationStructureDeviceAddressKHR
      pvkGetAccelerationStructureDeviceAddressKHR =
          (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(
              deviceHandle, "vkGetAccelerationStructureDeviceAddressKHR");

  PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR =
      (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(
          deviceHandle, "vkCmdBuildAccelerationStructuresKHR");

  VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .pNext = NULL,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
      .deviceMask = 0};

  // =========================================================================
  // Command Pool

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

  // =========================================================================
  // Command Buffers

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

  // =========================================================================
  // Surface Features

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

  // =========================================================================
  // Swapchain

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
      .imageUsage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
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

  // =========================================================================
  // Render Pass

  std::vector<VkAttachmentDescription> attachmentDescriptionList = {
      {.flags = 0,
       .format = surfaceFormatList[0].format,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
      {.flags = 0,
       .format = VK_FORMAT_D32_SFLOAT,
       .samples = VK_SAMPLE_COUNT_1_BIT,
       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
       .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
       .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
       .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

  std::vector<VkAttachmentReference> attachmentReferenceList = {
      {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      {.attachment = 1,
       .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpassDescription = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentReferenceList[0],
      .pResolveAttachments = NULL,
      .pDepthStencilAttachment = &attachmentReferenceList[1],
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

  // =========================================================================
  // Swapchain Images

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

  // =========================================================================
  // Swapchain Image Views, Depth Images, Framebuffers

  std::vector<VkImageView> swapchainImageViewHandleList(swapchainImageCount,
                                                        VK_NULL_HANDLE);

  std::vector<VkImage> depthImageHandleList(swapchainImageCount,
                                            VK_NULL_HANDLE);

  std::vector<VkDeviceMemory> depthImageDeviceMemoryHandleList(
      swapchainImageCount, VK_NULL_HANDLE);

  std::vector<VkImageView> depthImageViewHandleList(swapchainImageCount,
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

    VkImageCreateInfo depthImageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = {.width = surfaceCapabilities.currentExtent.width,
                   .height = surfaceCapabilities.currentExtent.height,
                   .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    result = vkCreateImage(deviceHandle, &depthImageCreateInfo, NULL,
                           &depthImageHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateImage");
    }

    VkMemoryRequirements depthImageMemoryRequirements;
    vkGetImageMemoryRequirements(deviceHandle, depthImageHandleList[x],
                                 &depthImageMemoryRequirements);

    uint32_t depthImageMemoryTypeIndex = -1;
    for (uint32_t y = 0; y < physicalDeviceMemoryProperties.memoryTypeCount;
         y++) {
      if ((depthImageMemoryRequirements.memoryTypeBits & (1 << y)) &&
          (physicalDeviceMemoryProperties.memoryTypes[y].propertyFlags &
           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

        depthImageMemoryTypeIndex = y;
        break;
      }
    }

    VkMemoryAllocateInfo depthImageMemoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = depthImageMemoryRequirements.size,
        .memoryTypeIndex = depthImageMemoryTypeIndex};

    result = vkAllocateMemory(deviceHandle, &depthImageMemoryAllocateInfo, NULL,
                              &depthImageDeviceMemoryHandleList[x]);
    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkAllocateMemory");
    }

    result = vkBindImageMemory(deviceHandle, depthImageHandleList[x],
                               depthImageDeviceMemoryHandleList[x], 0);
    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkBindImageMemory");
    }

    VkImageViewCreateInfo depthImageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = depthImageHandleList[x],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    result = vkCreateImageView(deviceHandle, &depthImageViewCreateInfo, NULL,
                               &depthImageViewHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateImageView");
    }

    std::vector<VkImageView> imageViewHandleList = {
        swapchainImageViewHandleList[x], depthImageViewHandleList[x]};

    VkFramebufferCreateInfo framebufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = renderPassHandle,
        .attachmentCount = 2,
        .pAttachments = imageViewHandleList.data(),
        .width = surfaceCapabilities.currentExtent.width,
        .height = surfaceCapabilities.currentExtent.height,
        .layers = 1};

    result = vkCreateFramebuffer(deviceHandle, &framebufferCreateInfo, NULL,
                                 &framebufferHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateFramebuffer");
    }
  }

  // =========================================================================
  // Descriptor Pool

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

  // =========================================================================
  // Descriptor Set Layout

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

  // =========================================================================
  // Material Descriptor Set Layout

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

  // =========================================================================
  // Allocate Descriptor Sets

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

  // =========================================================================
  // Pipeline Layout

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

  // =========================================================================
  // Vertex Shader Module

  std::ifstream vertexFile("shaders/shader.vert.spv",
                           std::ios::binary | std::ios::ate);
  std::streamsize vertexFileSize = vertexFile.tellg();
  vertexFile.seekg(0, std::ios::beg);
  std::vector<uint32_t> vertexShaderSource(vertexFileSize / sizeof(uint32_t));

  vertexFile.read(reinterpret_cast<char *>(vertexShaderSource.data()),
                  vertexFileSize);

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

  // =========================================================================
  // Fragment Shader Module

  std::ifstream fragmentFile("shaders/shader.frag.spv",
                             std::ios::binary | std::ios::ate);
  std::streamsize fragmentFileSize = fragmentFile.tellg();
  fragmentFile.seekg(0, std::ios::beg);
  std::vector<uint32_t> fragmentShaderSource(fragmentFileSize /
                                             sizeof(uint32_t));

  fragmentFile.read(reinterpret_cast<char *>(fragmentShaderSource.data()),
                    fragmentFileSize);

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

  // =========================================================================
  // Graphics Pipeline

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
      .y = (float)surfaceCapabilities.currentExtent.height,
      .width = (float)surfaceCapabilities.currentExtent.width,
      .height = -(float)surfaceCapabilities.currentExtent.height,
      .minDepth = 0,
      .maxDepth = 1};

  VkRect2D screenRect2D = {.offset = {.x = 0, .y = 0},
                           .extent = surfaceCapabilities.currentExtent};

  VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &screenRect2D};

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

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = 0.0,
      .maxDepthBounds = 1.0};

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
      .pDepthStencilState = &pipelineDepthStencilStateCreateInfo,
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

  // =========================================================================
  // OBJ Model

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

  // =========================================================================
  // Vertex Buffer

  VkBufferCreateInfo vertexBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(float) * attrib.vertices.size() * 3,
      .usage =
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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

  // =========================================================================
  // Index Buffer

  VkBufferCreateInfo indexBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(uint32_t) * indexList.size(),
      .usage =
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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

  // =========================================================================
  // Bottom Level Acceleration Structure

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

  // =========================================================================
  // Build Bottom Level Acceleration Structure

  VkAccelerationStructureDeviceAddressInfoKHR
      bottomLevelAccelerationStructureDeviceAddressInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
          .pNext = NULL,
          .accelerationStructure = bottomLevelAccelerationStructureHandle};

  VkDeviceAddress bottomLevelAccelerationStructureDeviceAddress =
      pvkGetAccelerationStructureDeviceAddressKHR(
          deviceHandle, &bottomLevelAccelerationStructureDeviceAddressInfo);

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

  VkCommandBufferBeginInfo bottomLevelCommandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL};

  result = vkBeginCommandBuffer(commandBufferHandleList.back(),
                                &bottomLevelCommandBufferBeginInfo);

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

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateFence");
  }

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

  // =========================================================================
  // Top Level Acceleration Structure

  VkAccelerationStructureInstanceKHR bottomLevelAccelerationStructureInstance =
      {.transform = {.matrix = {{1.0, 0.0, 0.0, 0.0},
                                {0.0, 1.0, 0.0, 0.0},
                                {0.0, 0.0, 1.0, 0.0}}},
       .instanceCustomIndex = 0,
       .mask = 0xFF,
       .instanceShaderBindingTableRecordOffset = 0,
       .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
       .accelerationStructureReference =
           bottomLevelAccelerationStructureDeviceAddress};

  VkBufferCreateInfo bottomLevelGeometryInstanceBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(VkAccelerationStructureInstanceKHR),
      .usage =
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer bottomLevelGeometryInstanceBufferHandle = VK_NULL_HANDLE;
  result =
      vkCreateBuffer(deviceHandle, &bottomLevelGeometryInstanceBufferCreateInfo,
                     NULL, &bottomLevelGeometryInstanceBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements bottomLevelGeometryInstanceMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle,
                                bottomLevelGeometryInstanceBufferHandle,
                                &bottomLevelGeometryInstanceMemoryRequirements);

  uint32_t bottomLevelGeometryInstanceMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {

    if ((bottomLevelGeometryInstanceMemoryRequirements.memoryTypeBits &
         (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      bottomLevelGeometryInstanceMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo bottomLevelGeometryInstanceMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = bottomLevelGeometryInstanceMemoryRequirements.size,
      .memoryTypeIndex = bottomLevelGeometryInstanceMemoryTypeIndex};

  VkDeviceMemory bottomLevelGeometryInstanceDeviceMemoryHandle = VK_NULL_HANDLE;

  result = vkAllocateMemory(
      deviceHandle, &bottomLevelGeometryInstanceMemoryAllocateInfo, NULL,
      &bottomLevelGeometryInstanceDeviceMemoryHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result =
      vkBindBufferMemory(deviceHandle, bottomLevelGeometryInstanceBufferHandle,
                         bottomLevelGeometryInstanceDeviceMemoryHandle, 0);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostbottomLevelGeometryInstanceMemoryBuffer;
  result =
      vkMapMemory(deviceHandle, bottomLevelGeometryInstanceDeviceMemoryHandle,
                  0, sizeof(VkAccelerationStructureInstanceKHR), 0,
                  &hostbottomLevelGeometryInstanceMemoryBuffer);

  memcpy(hostbottomLevelGeometryInstanceMemoryBuffer,
         &bottomLevelAccelerationStructureInstance,
         sizeof(VkAccelerationStructureInstanceKHR));

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, bottomLevelGeometryInstanceDeviceMemoryHandle);

  VkBufferDeviceAddressInfo bottomLevelGeometryInstanceDeviceAddressInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .pNext = NULL,
      .buffer = bottomLevelGeometryInstanceBufferHandle};

  VkDeviceAddress bottomLevelGeometryInstanceDeviceAddress =
      pvkGetBufferDeviceAddressKHR(
          deviceHandle, &bottomLevelGeometryInstanceDeviceAddressInfo);

  VkAccelerationStructureGeometryDataKHR topLevelAccelerationStructureGeometryData =
      {.instances = {
           .sType =
               VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
           .pNext = NULL,
           .arrayOfPointers = VK_FALSE,
           .data = {.deviceAddress =
                        bottomLevelGeometryInstanceDeviceAddress}}};

  VkAccelerationStructureGeometryKHR topLevelAccelerationStructureGeometry = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .pNext = NULL,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = topLevelAccelerationStructureGeometryData,
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR};

  VkAccelerationStructureBuildGeometryInfoKHR
      topLevelAccelerationStructureBuildGeometryInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
          .pNext = NULL,
          .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
          .flags = 0,
          .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
          .srcAccelerationStructure = VK_NULL_HANDLE,
          .dstAccelerationStructure = VK_NULL_HANDLE,
          .geometryCount = 1,
          .pGeometries = &topLevelAccelerationStructureGeometry,
          .ppGeometries = NULL,
          .scratchData = {.deviceAddress = 0}};

  VkAccelerationStructureBuildSizesInfoKHR
      topLevelAccelerationStructureBuildSizesInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
          .pNext = NULL,
          .accelerationStructureSize = 0,
          .updateScratchSize = 0,
          .buildScratchSize = 0};

  std::vector<uint32_t> topLevelMaxPrimitiveCountList = {1};

  pvkGetAccelerationStructureBuildSizesKHR(
      deviceHandle, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &topLevelAccelerationStructureBuildGeometryInfo,
      topLevelMaxPrimitiveCountList.data(),
      &topLevelAccelerationStructureBuildSizesInfo);

  VkBufferCreateInfo topLevelAccelerationStructureBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size =
          topLevelAccelerationStructureBuildSizesInfo.accelerationStructureSize,
      .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer topLevelAccelerationStructureBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle,
                          &topLevelAccelerationStructureBufferCreateInfo, NULL,
                          &topLevelAccelerationStructureBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements topLevelAccelerationStructureMemoryRequirements;
  vkGetBufferMemoryRequirements(
      deviceHandle, topLevelAccelerationStructureBufferHandle,
      &topLevelAccelerationStructureMemoryRequirements);

  uint32_t topLevelAccelerationStructureMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {

    if ((topLevelAccelerationStructureMemoryRequirements.memoryTypeBits &
         (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

      topLevelAccelerationStructureMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo topLevelAccelerationStructureMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,
      .allocationSize = topLevelAccelerationStructureMemoryRequirements.size,
      .memoryTypeIndex = topLevelAccelerationStructureMemoryTypeIndex};

  VkDeviceMemory topLevelAccelerationStructureDeviceMemoryHandle =
      VK_NULL_HANDLE;

  result = vkAllocateMemory(
      deviceHandle, &topLevelAccelerationStructureMemoryAllocateInfo, NULL,
      &topLevelAccelerationStructureDeviceMemoryHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(
      deviceHandle, topLevelAccelerationStructureBufferHandle,
      topLevelAccelerationStructureDeviceMemoryHandle, 0);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  VkAccelerationStructureCreateInfoKHR topLevelAccelerationStructureCreateInfo =
      {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
       .pNext = NULL,
       .createFlags = 0,
       .buffer = topLevelAccelerationStructureBufferHandle,
       .offset = 0,
       .size = topLevelAccelerationStructureBuildSizesInfo
                   .accelerationStructureSize,
       .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
       .deviceAddress = 0};

  VkAccelerationStructureKHR topLevelAccelerationStructureHandle =
      VK_NULL_HANDLE;

  result = pvkCreateAccelerationStructureKHR(
      deviceHandle, &topLevelAccelerationStructureCreateInfo, NULL,
      &topLevelAccelerationStructureHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateAccelerationStructureKHR");
  }

  // =========================================================================
  // Build Top Level Acceleration Structure

  VkAccelerationStructureDeviceAddressInfoKHR
      topLevelAccelerationStructureDeviceAddressInfo = {
          .sType =
              VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
          .pNext = NULL,
          .accelerationStructure = topLevelAccelerationStructureHandle};

  VkDeviceAddress topLevelAccelerationStructureDeviceAddress =
      pvkGetAccelerationStructureDeviceAddressKHR(
          deviceHandle, &topLevelAccelerationStructureDeviceAddressInfo);

  VkBufferCreateInfo topLevelAccelerationStructureScratchBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = topLevelAccelerationStructureBuildSizesInfo.buildScratchSize,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer topLevelAccelerationStructureScratchBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(
      deviceHandle, &topLevelAccelerationStructureScratchBufferCreateInfo, NULL,
      &topLevelAccelerationStructureScratchBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements topLevelAccelerationStructureScratchMemoryRequirements;
  vkGetBufferMemoryRequirements(
      deviceHandle, topLevelAccelerationStructureScratchBufferHandle,
      &topLevelAccelerationStructureScratchMemoryRequirements);

  uint32_t topLevelAccelerationStructureScratchMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {

    if ((topLevelAccelerationStructureScratchMemoryRequirements.memoryTypeBits &
         (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

      topLevelAccelerationStructureScratchMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo topLevelAccelerationStructureScratchMemoryAllocateInfo =
      {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
       .pNext = &memoryAllocateFlagsInfo,
       .allocationSize =
           topLevelAccelerationStructureScratchMemoryRequirements.size,
       .memoryTypeIndex = topLevelAccelerationStructureScratchMemoryTypeIndex};

  VkDeviceMemory topLevelAccelerationStructureDeviceScratchMemoryHandle =
      VK_NULL_HANDLE;

  result = vkAllocateMemory(
      deviceHandle, &topLevelAccelerationStructureScratchMemoryAllocateInfo,
      NULL, &topLevelAccelerationStructureDeviceScratchMemoryHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(
      deviceHandle, topLevelAccelerationStructureScratchBufferHandle,
      topLevelAccelerationStructureDeviceScratchMemoryHandle, 0);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  VkBufferDeviceAddressInfo
      topLevelAccelerationStructureScratchBufferDeviceAddressInfo = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          .pNext = NULL,
          .buffer = topLevelAccelerationStructureScratchBufferHandle};

  VkDeviceAddress topLevelAccelerationStructureScratchBufferDeviceAddress =
      pvkGetBufferDeviceAddressKHR(
          deviceHandle,
          &topLevelAccelerationStructureScratchBufferDeviceAddressInfo);

  topLevelAccelerationStructureBuildGeometryInfo.dstAccelerationStructure =
      topLevelAccelerationStructureHandle;

  topLevelAccelerationStructureBuildGeometryInfo.scratchData = {
      .deviceAddress = topLevelAccelerationStructureScratchBufferDeviceAddress};

  VkAccelerationStructureBuildRangeInfoKHR
      topLevelAccelerationStructureBuildRangeInfo = {.primitiveCount = 1,
                                                     .primitiveOffset = 0,
                                                     .firstVertex = 0,
                                                     .transformOffset = 0};

  const VkAccelerationStructureBuildRangeInfoKHR
      *topLevelAccelerationStructureBuildRangeInfos =
          &topLevelAccelerationStructureBuildRangeInfo;

  VkCommandBufferBeginInfo topLevelCommandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL};

  result = vkBeginCommandBuffer(commandBufferHandleList.back(),
                                &topLevelCommandBufferBeginInfo);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBeginCommandBuffer");
  }

  pvkCmdBuildAccelerationStructuresKHR(
      commandBufferHandleList.back(), 1,
      &topLevelAccelerationStructureBuildGeometryInfo,
      &topLevelAccelerationStructureBuildRangeInfos);

  result = vkEndCommandBuffer(commandBufferHandleList.back());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEndCommandBuffer");
  }

  VkSubmitInfo topLevelAccelerationStructureBuildSubmitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = NULL,
      .pWaitDstStageMask = NULL,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferHandleList.back(),
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = NULL};

  VkFenceCreateInfo topLevelAccelerationStructureBuildFenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = 0};

  VkFence topLevelAccelerationStructureBuildFenceHandle = VK_NULL_HANDLE;
  result = vkCreateFence(deviceHandle,
                         &topLevelAccelerationStructureBuildFenceCreateInfo,
                         NULL, &topLevelAccelerationStructureBuildFenceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateFence");
  }

  result = vkQueueSubmit(queueHandle, 1,
                         &topLevelAccelerationStructureBuildSubmitInfo,
                         topLevelAccelerationStructureBuildFenceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkQueueSubmit");
  }

  result = vkWaitForFences(deviceHandle, 1,
                           &topLevelAccelerationStructureBuildFenceHandle, true,
                           UINT32_MAX);

  if (result != VK_SUCCESS && result != VK_TIMEOUT) {
    throwExceptionVulkanAPI(result, "vkWaitForFences");
  }

  // =========================================================================
  // Uniform Buffer

  struct UniformStructure {
    float cameraPosition[4] = {0, 0, 0, 1};
    float cameraRight[4] = {1, 0, 0, 1};
    float cameraUp[4] = {0, 1, 0, 1};
    float cameraForward[4] = {0, 0, 1, 1};

    uint32_t frameCount = 0;
  } uniformStructure;

  VkBufferCreateInfo uniformBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(UniformStructure),
      .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer uniformBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle, &uniformBufferCreateInfo, NULL,
                          &uniformBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements uniformMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle, uniformBufferHandle,
                                &uniformMemoryRequirements);

  uint32_t uniformMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((uniformMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      uniformMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo uniformMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = uniformMemoryRequirements.size,
      .memoryTypeIndex = uniformMemoryTypeIndex};

  VkDeviceMemory uniformDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &uniformMemoryAllocateInfo, NULL,
                            &uniformDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(deviceHandle, uniformBufferHandle,
                              uniformDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostUniformMemoryBuffer;
  result = vkMapMemory(deviceHandle, uniformDeviceMemoryHandle, 0,
                       sizeof(UniformStructure), 0, &hostUniformMemoryBuffer);

  memcpy(hostUniformMemoryBuffer, &uniformStructure, sizeof(UniformStructure));

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, uniformDeviceMemoryHandle);

  // =========================================================================
  // Ray Trace Image

  VkImageCreateInfo rayTraceImageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = surfaceFormatList[0].format,
      .extent = {.width = surfaceCapabilities.currentExtent.width,
                 .height = surfaceCapabilities.currentExtent.height,
                 .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  VkImage rayTraceImageHandle = VK_NULL_HANDLE;
  result = vkCreateImage(deviceHandle, &rayTraceImageCreateInfo, NULL,
                         &rayTraceImageHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateImage");
  }

  VkMemoryRequirements rayTraceImageMemoryRequirements;
  vkGetImageMemoryRequirements(deviceHandle, rayTraceImageHandle,
                               &rayTraceImageMemoryRequirements);

  uint32_t rayTraceImageMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((rayTraceImageMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {

      rayTraceImageMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo rayTraceImageMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = NULL,
      .allocationSize = rayTraceImageMemoryRequirements.size,
      .memoryTypeIndex = rayTraceImageMemoryTypeIndex};

  VkDeviceMemory rayTraceImageDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &rayTraceImageMemoryAllocateInfo,
                            NULL, &rayTraceImageDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindImageMemory(deviceHandle, rayTraceImageHandle,
                             rayTraceImageDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindImageMemory");
  }

  VkImageViewCreateInfo rayTraceImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .image = rayTraceImageHandle,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = surfaceFormatList[0].format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  VkImageView rayTraceImageViewHandle = VK_NULL_HANDLE;
  result = vkCreateImageView(deviceHandle, &rayTraceImageViewCreateInfo, NULL,
                             &rayTraceImageViewHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateImageView");
  }

  // =========================================================================
  // Ray Trace Image Barrier
  // (VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_GENERAL)

  VkCommandBufferBeginInfo rayTraceImageBarrierCommandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL};

  result = vkBeginCommandBuffer(commandBufferHandleList.back(),
                                &rayTraceImageBarrierCommandBufferBeginInfo);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBeginCommandBuffer");
  }

  VkImageMemoryBarrier rayTraceGeneralMemoryBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = 0,
      .dstAccessMask = 0,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = queueFamilyIndex,
      .dstQueueFamilyIndex = queueFamilyIndex,
      .image = rayTraceImageHandle,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  vkCmdPipelineBarrier(commandBufferHandleList.back(),
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL,
                       1, &rayTraceGeneralMemoryBarrier);

  result = vkEndCommandBuffer(commandBufferHandleList.back());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkEndCommandBuffer");
  }

  VkSubmitInfo rayTraceImageBarrierAccelerationStructureBuildSubmitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = NULL,
      .pWaitDstStageMask = NULL,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferHandleList.back(),
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = NULL};

  VkFenceCreateInfo
      rayTraceImageBarrierAccelerationStructureBuildFenceCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .pNext = NULL,
          .flags = 0};

  VkFence rayTraceImageBarrierAccelerationStructureBuildFenceHandle =
      VK_NULL_HANDLE;
  result = vkCreateFence(
      deviceHandle,
      &rayTraceImageBarrierAccelerationStructureBuildFenceCreateInfo, NULL,
      &rayTraceImageBarrierAccelerationStructureBuildFenceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateFence");
  }

  result = vkQueueSubmit(
      queueHandle, 1, &rayTraceImageBarrierAccelerationStructureBuildSubmitInfo,
      rayTraceImageBarrierAccelerationStructureBuildFenceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkQueueSubmit");
  }

  result = vkWaitForFences(
      deviceHandle, 1,
      &rayTraceImageBarrierAccelerationStructureBuildFenceHandle, true,
      UINT32_MAX);

  if (result != VK_SUCCESS && result != VK_TIMEOUT) {
    throwExceptionVulkanAPI(result, "vkWaitForFences");
  }

  // =========================================================================
  // Update Descriptor Set

  VkWriteDescriptorSetAccelerationStructureKHR
      accelerationStructureDescriptorInfo = {
          .sType =
              VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
          .pNext = NULL,
          .accelerationStructureCount = 1,
          .pAccelerationStructures = &topLevelAccelerationStructureHandle};

  VkDescriptorBufferInfo uniformDescriptorInfo = {
      .buffer = uniformBufferHandle, .offset = 0, .range = VK_WHOLE_SIZE};

  VkDescriptorBufferInfo indexDescriptorInfo = {
      .buffer = indexBufferHandle, .offset = 0, .range = VK_WHOLE_SIZE};

  VkDescriptorBufferInfo vertexDescriptorInfo = {
      .buffer = vertexBufferHandle, .offset = 0, .range = VK_WHOLE_SIZE};

  VkDescriptorImageInfo rayTraceImageDescriptorInfo = {
      .sampler = VK_NULL_HANDLE,
      .imageView = rayTraceImageViewHandle,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writeDescriptorSetList = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = &accelerationStructureDescriptorInfo,
       .dstSet = descriptorSetHandleList[0],
       .dstBinding = 0,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
       .pImageInfo = NULL,
       .pBufferInfo = NULL,
       .pTexelBufferView = NULL},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[0],
       .dstBinding = 1,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
       .pImageInfo = NULL,
       .pBufferInfo = &uniformDescriptorInfo,
       .pTexelBufferView = NULL},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[0],
       .dstBinding = 2,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pImageInfo = NULL,
       .pBufferInfo = &indexDescriptorInfo,
       .pTexelBufferView = NULL},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[0],
       .dstBinding = 3,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pImageInfo = NULL,
       .pBufferInfo = &vertexDescriptorInfo,
       .pTexelBufferView = NULL},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[0],
       .dstBinding = 4,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       .pImageInfo = &rayTraceImageDescriptorInfo,
       .pBufferInfo = NULL,
       .pTexelBufferView = NULL}};

  vkUpdateDescriptorSets(deviceHandle, writeDescriptorSetList.size(),
                         writeDescriptorSetList.data(), 0, NULL);

  // =========================================================================
  // Material Index Buffer

  std::vector<uint32_t> materialIndexList;
  for (tinyobj::shape_t shape : shapes) {
    for (int index : shape.mesh.material_ids) {
      materialIndexList.push_back(index);
    }
  }

  VkBufferCreateInfo materialIndexBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(uint32_t) * materialIndexList.size(),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer materialIndexBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle, &materialIndexBufferCreateInfo, NULL,
                          &materialIndexBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements materialIndexMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle, materialIndexBufferHandle,
                                &materialIndexMemoryRequirements);

  uint32_t materialIndexMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((materialIndexMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      materialIndexMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo materialIndexMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = materialIndexMemoryRequirements.size,
      .memoryTypeIndex = materialIndexMemoryTypeIndex};

  VkDeviceMemory materialIndexDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &materialIndexMemoryAllocateInfo,
                            NULL, &materialIndexDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(deviceHandle, materialIndexBufferHandle,
                              materialIndexDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostMaterialIndexMemoryBuffer;
  result = vkMapMemory(deviceHandle, materialIndexDeviceMemoryHandle, 0,
                       sizeof(uint32_t) * materialIndexList.size(), 0,
                       &hostMaterialIndexMemoryBuffer);

  memcpy(hostMaterialIndexMemoryBuffer, materialIndexList.data(),
         sizeof(uint32_t) * materialIndexList.size());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, materialIndexDeviceMemoryHandle);

  // =========================================================================
  // Material Buffer

  struct Material {
    float ambient[4] = {0, 0, 0, 0};
    float diffuse[4] = {0, 0, 0, 0};
    float specular[4] = {0, 0, 0, 0};
    float emission[4] = {0, 0, 0, 0};
  };

  std::vector<Material> materialList(materials.size());
  for (uint32_t x = 0; x < materials.size(); x++) {
    memcpy(materialList[x].ambient, materials[x].ambient, sizeof(float) * 3);
    memcpy(materialList[x].diffuse, materials[x].diffuse, sizeof(float) * 3);
    memcpy(materialList[x].specular, materials[x].specular, sizeof(float) * 3);
    memcpy(materialList[x].emission, materials[x].emission, sizeof(float) * 3);
  }

  VkBufferCreateInfo materialBufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .size = sizeof(Material) * materialList.size(),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamilyIndex};

  VkBuffer materialBufferHandle = VK_NULL_HANDLE;
  result = vkCreateBuffer(deviceHandle, &materialBufferCreateInfo, NULL,
                          &materialBufferHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkCreateBuffer");
  }

  VkMemoryRequirements materialMemoryRequirements;
  vkGetBufferMemoryRequirements(deviceHandle, materialBufferHandle,
                                &materialMemoryRequirements);

  uint32_t materialMemoryTypeIndex = -1;
  for (uint32_t x = 0; x < physicalDeviceMemoryProperties.memoryTypeCount;
       x++) {
    if ((materialMemoryRequirements.memoryTypeBits & (1 << x)) &&
        (physicalDeviceMemoryProperties.memoryTypes[x].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ==
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {

      materialMemoryTypeIndex = x;
      break;
    }
  }

  VkMemoryAllocateInfo materialMemoryAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memoryAllocateFlagsInfo,
      .allocationSize = materialMemoryRequirements.size,
      .memoryTypeIndex = materialMemoryTypeIndex};

  VkDeviceMemory materialDeviceMemoryHandle = VK_NULL_HANDLE;
  result = vkAllocateMemory(deviceHandle, &materialMemoryAllocateInfo, NULL,
                            &materialDeviceMemoryHandle);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateMemory");
  }

  result = vkBindBufferMemory(deviceHandle, materialBufferHandle,
                              materialDeviceMemoryHandle, 0);
  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkBindBufferMemory");
  }

  void *hostMaterialMemoryBuffer;
  result = vkMapMemory(deviceHandle, materialDeviceMemoryHandle, 0,
                       sizeof(Material) * materialList.size(), 0,
                       &hostMaterialMemoryBuffer);

  memcpy(hostMaterialMemoryBuffer, materialList.data(),
         sizeof(Material) * materialList.size());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkMapMemory");
  }

  vkUnmapMemory(deviceHandle, materialDeviceMemoryHandle);

  // =========================================================================
  // Update Material Descriptor Set

  VkDescriptorBufferInfo materialIndexDescriptorInfo = {
      .buffer = materialIndexBufferHandle, .offset = 0, .range = VK_WHOLE_SIZE};

  VkDescriptorBufferInfo materialDescriptorInfo = {
      .buffer = materialBufferHandle, .offset = 0, .range = VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> materialWriteDescriptorSetList = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[1],
       .dstBinding = 0,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pImageInfo = NULL,
       .pBufferInfo = &materialIndexDescriptorInfo,
       .pTexelBufferView = NULL},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .pNext = NULL,
       .dstSet = descriptorSetHandleList[1],
       .dstBinding = 1,
       .dstArrayElement = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pImageInfo = NULL,
       .pBufferInfo = &materialDescriptorInfo,
       .pTexelBufferView = NULL}};

  vkUpdateDescriptorSets(deviceHandle, materialWriteDescriptorSetList.size(),
                         materialWriteDescriptorSetList.data(), 0, NULL);

  // =========================================================================
  // Record Render Pass Command Buffers

  for (uint32_t x = 0; x < swapchainImageCount; x++) {
    VkCommandBufferBeginInfo renderCommandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
        .pInheritanceInfo = NULL};

    result = vkBeginCommandBuffer(commandBufferHandleList[x],
                                  &renderCommandBufferBeginInfo);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkBeginCommandBuffer");
    }

    std::vector<VkClearValue> clearValueList = {
        {.color = {0.0f, 0.0f, 0.0f, 1.0f}}, {.depthStencil = {1.0f, 0}}};

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = renderPassHandle,
        .framebuffer = framebufferHandleList[x],
        .renderArea = screenRect2D,
        .clearValueCount = (uint32_t)clearValueList.size(),
        .pClearValues = clearValueList.data()};

    vkCmdBeginRenderPass(commandBufferHandleList[x], &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBufferHandleList[x],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineHandle);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBufferHandleList[x], 0, 1,
                           &vertexBufferHandle, &offset);

    vkCmdBindIndexBuffer(commandBufferHandleList[x], indexBufferHandle, 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(
        commandBufferHandleList[x], VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayoutHandle, 0, (uint32_t)descriptorSetHandleList.size(),
        descriptorSetHandleList.data(), 0, NULL);

    vkCmdDrawIndexed(commandBufferHandleList[x], indexList.size(), 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBufferHandleList[x]);

    VkImageMemoryBarrier swapchainCopyMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = swapchainImageHandleList[x],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    vkCmdPipelineBarrier(commandBufferHandleList[x],
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                         NULL, 1, &swapchainCopyMemoryBarrier);

    VkImageMemoryBarrier rayTraceCopyMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = rayTraceImageHandle,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    vkCmdPipelineBarrier(commandBufferHandleList[x],
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                         NULL, 1, &rayTraceCopyMemoryBarrier);

    VkImageCopy imageCopy = {
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .srcOffset = {.x = 0, .y = 0, .z = 0},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .dstOffset = {.x = 0, .y = 0, .z = 0},
        .extent = {.width = surfaceCapabilities.currentExtent.width,
                   .height = surfaceCapabilities.currentExtent.height,
                   .depth = 1}};

    vkCmdCopyImage(commandBufferHandleList[x], swapchainImageHandleList[x],
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rayTraceImageHandle,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    VkImageMemoryBarrier swapchainPresentMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = swapchainImageHandleList[x],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    vkCmdPipelineBarrier(commandBufferHandleList[x],
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                         NULL, 1, &swapchainPresentMemoryBarrier);

    VkImageMemoryBarrier rayTraceWriteMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = rayTraceImageHandle,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};

    vkCmdPipelineBarrier(commandBufferHandleList[x],
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                         NULL, 1, &rayTraceWriteMemoryBarrier);

    result = vkEndCommandBuffer(commandBufferHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkEndCommandBuffer");
    }
  }

  // =========================================================================
  // Fences, Semaphores

  std::vector<VkFence> imageAvailableFenceHandleList(swapchainImageCount,
                                                     VK_NULL_HANDLE);

  std::vector<VkSemaphore> acquireImageSemaphoreHandleList(swapchainImageCount,
                                                           VK_NULL_HANDLE);

  std::vector<VkSemaphore> writeImageSemaphoreHandleList(swapchainImageCount,
                                                         VK_NULL_HANDLE);

  for (uint32_t x = 0; x < swapchainImageCount; x++) {
    VkFenceCreateInfo imageAvailableFenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    result = vkCreateFence(deviceHandle, &imageAvailableFenceCreateInfo, NULL,
                           &imageAvailableFenceHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateFence");
    }

    VkSemaphoreCreateInfo acquireImageSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0};

    result = vkCreateSemaphore(deviceHandle, &acquireImageSemaphoreCreateInfo,
                               NULL, &acquireImageSemaphoreHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateSemaphore");
    }

    VkSemaphoreCreateInfo writeImageSemaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0};

    result = vkCreateSemaphore(deviceHandle, &writeImageSemaphoreCreateInfo,
                               NULL, &writeImageSemaphoreHandleList[x]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkCreateSemaphore");
    }
  }

  // =========================================================================
  // Main Loop

  uint32_t currentFrame = 0;

  while (!glfwWindowShouldClose(windowPtr)) {
    glfwPollEvents();

    bool isCameraMoved = false;

    if (keyDownIndex[GLFW_KEY_W]) {
      cameraPosition[0] += cos(-cameraYaw - (M_PI / 2)) * 0.01f;
      cameraPosition[2] += sin(-cameraYaw - (M_PI / 2)) * 0.01f;
      isCameraMoved = true;
    }
    if (keyDownIndex[GLFW_KEY_S]) {
      cameraPosition[0] -= cos(-cameraYaw - (M_PI / 2)) * 0.01f;
      cameraPosition[2] -= sin(-cameraYaw - (M_PI / 2)) * 0.01f;
      isCameraMoved = true;
    }
    if (keyDownIndex[GLFW_KEY_A]) {
      cameraPosition[0] -= cos(-cameraYaw) * 0.01f;
      cameraPosition[2] -= sin(-cameraYaw) * 0.01f;
      isCameraMoved = true;
    }
    if (keyDownIndex[GLFW_KEY_D]) {
      cameraPosition[0] += cos(-cameraYaw) * 0.01f;
      cameraPosition[2] += sin(-cameraYaw) * 0.01f;
      isCameraMoved = true;
    }
    if (keyDownIndex[GLFW_KEY_SPACE]) {
      cameraPosition[1] += 0.01f;
      isCameraMoved = true;
    }
    if (keyDownIndex[GLFW_KEY_LEFT_CONTROL]) {
      cameraPosition[1] -= 0.01f;
      isCameraMoved = true;
    }

    static double previousMousePositionX;

    double xPos, yPos;
    glfwGetCursorPos(windowPtr, &xPos, &yPos);

    if (previousMousePositionX != xPos) {
      double mouseDifferenceX = previousMousePositionX - xPos;
      cameraYaw += mouseDifferenceX * 0.0005f;
      previousMousePositionX = xPos;

      isCameraMoved = 1;
    }

    if (isCameraMoved) {
      uniformStructure.cameraPosition[0] = cameraPosition[0];
      uniformStructure.cameraPosition[1] = cameraPosition[1];
      uniformStructure.cameraPosition[2] = cameraPosition[2];

      uniformStructure.cameraForward[0] =
          cosf(cameraPitch) * cosf(-cameraYaw - (M_PI / 2.0));
      uniformStructure.cameraForward[1] = sinf(cameraPitch);
      uniformStructure.cameraForward[2] =
          cosf(cameraPitch) * sinf(-cameraYaw - (M_PI / 2.0));

      uniformStructure.cameraRight[0] =
          uniformStructure.cameraForward[1] * uniformStructure.cameraUp[2] -
          uniformStructure.cameraForward[2] * uniformStructure.cameraUp[1];
      uniformStructure.cameraRight[1] =
          uniformStructure.cameraForward[2] * uniformStructure.cameraUp[0] -
          uniformStructure.cameraForward[0] * uniformStructure.cameraUp[2];
      uniformStructure.cameraRight[2] =
          uniformStructure.cameraForward[0] * uniformStructure.cameraUp[1] -
          uniformStructure.cameraForward[1] * uniformStructure.cameraUp[0];

      uniformStructure.frameCount = 0;
    } else {
      uniformStructure.frameCount += 1;
    }

    result = vkMapMemory(deviceHandle, uniformDeviceMemoryHandle, 0,
                         sizeof(UniformStructure), 0, &hostUniformMemoryBuffer);

    memcpy(hostUniformMemoryBuffer, &uniformStructure,
           sizeof(UniformStructure));

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkMapMemory");
    }

    vkUnmapMemory(deviceHandle, uniformDeviceMemoryHandle);

    result = vkWaitForFences(deviceHandle, 1,
                             &imageAvailableFenceHandleList[currentFrame], true,
                             UINT32_MAX);

    if (result != VK_SUCCESS && result != VK_TIMEOUT) {
      throwExceptionVulkanAPI(result, "vkWaitForFences");
    }

    result = vkResetFences(deviceHandle, 1,
                           &imageAvailableFenceHandleList[currentFrame]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkResetFences");
    }

    uint32_t currentImageIndex = -1;
    result =
        vkAcquireNextImageKHR(deviceHandle, swapchainHandle, UINT32_MAX,
                              acquireImageSemaphoreHandleList[currentFrame],
                              VK_NULL_HANDLE, &currentImageIndex);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkAcquireNextImageKHR");
    }

    VkPipelineStageFlags pipelineStageFlags =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquireImageSemaphoreHandleList[currentFrame],
        .pWaitDstStageMask = &pipelineStageFlags,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBufferHandleList[currentImageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &writeImageSemaphoreHandleList[currentImageIndex]};

    result = vkQueueSubmit(queueHandle, 1, &submitInfo,
                           imageAvailableFenceHandleList[currentFrame]);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkQueueSubmit");
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = NULL,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &writeImageSemaphoreHandleList[currentImageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchainHandle,
        .pImageIndices = &currentImageIndex,
        .pResults = NULL};

    result = vkQueuePresentKHR(queueHandle, &presentInfo);

    if (result != VK_SUCCESS) {
      throwExceptionVulkanAPI(result, "vkQueuePresentKHR");
    }

    currentFrame = (currentFrame + 1) % swapchainImageCount;
  }

  // =========================================================================
  // Cleanup

  result = vkDeviceWaitIdle(deviceHandle);

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkDeviceWaitIdle");
  }

  for (uint32_t x = 0; x < swapchainImageCount; x++) {
    vkDestroySemaphore(deviceHandle, writeImageSemaphoreHandleList[x], NULL);
    vkDestroySemaphore(deviceHandle, acquireImageSemaphoreHandleList[x], NULL);
    vkDestroyFence(deviceHandle, imageAvailableFenceHandleList[x], NULL);
  }

  vkFreeMemory(deviceHandle, materialDeviceMemoryHandle, NULL);
  vkDestroyBuffer(deviceHandle, materialBufferHandle, NULL);
  vkFreeMemory(deviceHandle, materialIndexDeviceMemoryHandle, NULL);
  vkDestroyBuffer(deviceHandle, materialIndexBufferHandle, NULL);
  vkDestroyFence(deviceHandle,
                 rayTraceImageBarrierAccelerationStructureBuildFenceHandle,
                 NULL);

  vkDestroyImageView(deviceHandle, rayTraceImageViewHandle, NULL);
  vkFreeMemory(deviceHandle, rayTraceImageDeviceMemoryHandle, NULL);
  vkDestroyImage(deviceHandle, rayTraceImageHandle, NULL);
  vkFreeMemory(deviceHandle, uniformDeviceMemoryHandle, NULL);
  vkDestroyBuffer(deviceHandle, uniformBufferHandle, NULL);
  vkDestroyFence(deviceHandle, topLevelAccelerationStructureBuildFenceHandle,
                 NULL);

  vkFreeMemory(deviceHandle,
               topLevelAccelerationStructureDeviceScratchMemoryHandle, NULL);

  vkDestroyBuffer(deviceHandle,
                  topLevelAccelerationStructureScratchBufferHandle, NULL);

  pvkDestroyAccelerationStructureKHR(deviceHandle,
                                     topLevelAccelerationStructureHandle, NULL);

  vkFreeMemory(deviceHandle, topLevelAccelerationStructureDeviceMemoryHandle,
               NULL);

  vkDestroyBuffer(deviceHandle, topLevelAccelerationStructureBufferHandle,
                  NULL);

  vkFreeMemory(deviceHandle, bottomLevelGeometryInstanceDeviceMemoryHandle,
               NULL);

  vkDestroyBuffer(deviceHandle, bottomLevelGeometryInstanceBufferHandle, NULL);
  vkDestroyFence(deviceHandle, bottomLevelAccelerationStructureBuildFenceHandle,
                 NULL);

  vkFreeMemory(deviceHandle,
               bottomLevelAccelerationStructureDeviceScratchMemoryHandle, NULL);

  vkDestroyBuffer(deviceHandle,
                  bottomLevelAccelerationStructureScratchBufferHandle, NULL);

  pvkDestroyAccelerationStructureKHR(
      deviceHandle, bottomLevelAccelerationStructureHandle, NULL);

  vkFreeMemory(deviceHandle, bottomLevelAccelerationStructureDeviceMemoryHandle,
               NULL);

  vkDestroyBuffer(deviceHandle, bottomLevelAccelerationStructureBufferHandle,
                  NULL);

  vkFreeMemory(deviceHandle, indexDeviceMemoryHandle, NULL);
  vkDestroyBuffer(deviceHandle, indexBufferHandle, NULL);
  vkFreeMemory(deviceHandle, vertexDeviceMemoryHandle, NULL);
  vkDestroyBuffer(deviceHandle, vertexBufferHandle, NULL);
  vkDestroyPipeline(deviceHandle, graphicsPipelineHandle, NULL);
  vkDestroyShaderModule(deviceHandle, fragmentShaderModuleHandle, NULL);
  vkDestroyShaderModule(deviceHandle, vertexShaderModuleHandle, NULL);
  vkDestroyPipelineLayout(deviceHandle, pipelineLayoutHandle, NULL);
  vkDestroyDescriptorSetLayout(deviceHandle, materialDescriptorSetLayoutHandle,
                               NULL);

  vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayoutHandle, NULL);
  vkDestroyDescriptorPool(deviceHandle, descriptorPoolHandle, NULL);

  for (uint32_t x = 0; x < swapchainImageCount; x++) {
    vkDestroyFramebuffer(deviceHandle, framebufferHandleList[x], NULL);
    vkDestroyImageView(deviceHandle, depthImageViewHandleList[x], NULL);
    vkFreeMemory(deviceHandle, depthImageDeviceMemoryHandleList[x], NULL);
    vkDestroyImage(deviceHandle, depthImageHandleList[x], NULL);
    vkDestroyImageView(deviceHandle, swapchainImageViewHandleList[x], NULL);
  }

  vkDestroyRenderPass(deviceHandle, renderPassHandle, NULL);
  vkDestroySwapchainKHR(deviceHandle, swapchainHandle, NULL);
  vkDestroyCommandPool(deviceHandle, commandPoolHandle, NULL);
  vkDestroyDevice(deviceHandle, NULL);
  vkDestroySurfaceKHR(instanceHandle, surfaceHandle, NULL);
  vkDestroyInstance(instanceHandle, NULL);

  return 0;
}
