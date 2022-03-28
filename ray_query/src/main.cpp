#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

#define STRING_RESET "\033[0m"
#define STRING_INFO "\033[37m"
#define STRING_WARNING "\033[33m"
#define STRING_ERROR "\033[36m"

#define PRINT_MESSAGE(stream, message) stream << message << std::endl;

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
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
  GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetKeyCallback(window, keyCallback);

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

  instanceExtensionList.push_back("VK_KHR_get_physical_device_properties2");
  instanceExtensionList.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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
      queueFamilyIndex = x;
      break;
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
      "VK_KHR_deferred_host_operations"};

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
      .pEnabledFeatures = NULL};

  VkDevice deviceHandle = VK_NULL_HANDLE;
  result = vkCreateDevice(activePhysicalDeviceHandle, &deviceCreateInfo,
                                   NULL, &deviceHandle);

  VkCommandPoolCreateInfo commandPoolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndex};

  VkCommandPool commandPoolHandle = VK_NULL_HANDLE;
  result = vkCreateCommandPool(deviceHandle, &commandPoolCreateInfo,
                                        NULL, &commandPoolHandle);

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

  result =
      vkAllocateCommandBuffers(deviceHandle, &commandBufferAllocateInfo,
                               commandBufferHandleList.data());

  if (result != VK_SUCCESS) {
    throwExceptionVulkanAPI(result, "vkAllocateCommandBuffers");
  }

  return -1;
}
