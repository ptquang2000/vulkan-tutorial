#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <print>
#include <ranges>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace utils {
template <std::ranges::input_range RequiredRange,
          std::ranges::input_range SupportedRange, typename Proj>
bool arePropertiesSupported(const RequiredRange& required,
                            const SupportedRange& supported, Proj proj) {
  return std::ranges::empty(required) ||
         std::ranges::all_of(
             required,
             [supported_names =
                  supported | std::views::transform(proj)](const auto& req) {
               return std::ranges::any_of(
                   supported_names, [req_view = std::string_view(req)](
                                        const auto& supported_elem) {
                     return std::string_view(supported_elem) == req_view;
                   });
             });
}

}  // namespace utils

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
  std::println("[VULKAN-VALIDATION] type {}, msg: {}", vk::to_string(type),
               pCallbackData->pMessage);
  return vk::False;
}

class HelloTriangleApplication {
  struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static constexpr vk::VertexInputBindingDescription getBindingDescription() {
      return {
          .binding = 0,
          .stride = sizeof(Vertex),
          .inputRate = vk::VertexInputRate::eVertex,
      };
    }

    static constexpr std::array<vk::VertexInputAttributeDescription, 2>
    getAttributeDescriptions() {
      return {
          vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat,
                                              offsetof(Vertex, pos)),
          vk::VertexInputAttributeDescription(
              1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
      };
    }
  };
  static constexpr std::array k_oldVertices = {
      Vertex{{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
      Vertex{{-0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
      Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

      Vertex{{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      Vertex{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},

      Vertex{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      Vertex{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      Vertex{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
  };
  static constexpr std::array k_vertices = {
      Vertex{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      Vertex{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
      Vertex{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      Vertex{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
  };

  static constexpr std::array<uint16_t, 6> k_indices = {
      0, 1, 2, 2, 3, 0,
  };

  struct UniformBufferObject {
    // alignas(8) glm::vec2 foo;
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
  };

 public:
  void run() {
    init();
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

 private:
  static constexpr vk::ApplicationInfo k_appInfo{
      .pApplicationName = "Hello Triangle",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = vk::ApiVersion14};
  static constexpr vk::QueueFlagBits k_queueFlag = vk::QueueFlagBits::eGraphics;
  static constexpr std::array k_validationLayers = {
#ifndef NDEBUG
      "VK_LAYER_KHRONOS_validation",
#endif
  };
  constexpr bool isDebugLayerEnabled() { return k_validationLayers.empty(); }

 private:
  void init() {
    std::string exe;
    exe.resize(PATH_MAX, '\0');
    auto count = readlink("/proc/self/exe", exe.data(), exe.size());
    if (count == -1) {
      throw std::runtime_error("Failed to locate exe.");
    }
    exe.resize(count + 1);
    const std::filesystem::path root = std::filesystem::path(exe).parent_path();
    m_texturesPath = root / "textures";
    m_shadersPath = root / "shaders";
  }

  void initWindow() {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInit();

    static constexpr uint32_t WIDTH = 640;
    static constexpr uint32_t HEIGHT = 480;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, onFrameBufferResized);
  }

  static void onFrameBufferResized(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<HelloTriangleApplication*>(
        glfwGetWindowUserPointer(window));
    app->m_frameBufferResized = true;
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    m_sharingMode = createLogicalDevice();
    createSwapChain();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandBuffers();
    createTextureImage();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createSyncObjects();
  }

  void createInstance() {
    std::vector<const char*> requiredLayers{};
    requiredLayers.assign(k_validationLayers.begin(), k_validationLayers.end());
    if (!utils::arePropertiesSupported(
            requiredLayers, m_context.enumerateInstanceLayerProperties(),
            &vk::LayerProperties::layerName)) {
      throw std::runtime_error(
          "One or more required layers are not supported!");
    }

    unsigned int glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions(
        glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (isDebugLayerEnabled()) {
      requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    if (!utils::arePropertiesSupported(
            std::span{&glfwExtensions[0], glfwExtensionCount},
            m_context.enumerateInstanceExtensionProperties(),
            &vk::ExtensionProperties::extensionName)) {
      throw std::runtime_error(
          "One or more required extension are not supported!");
    }

    m_instance = vk::raii::Instance(
        m_context,
        vk::InstanceCreateInfo{
            .pApplicationInfo = &k_appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions,
        });
  }

  void setupDebugMessenger() {
    if (!isDebugLayerEnabled()) return;

    m_debugger = m_instance.createDebugUtilsMessengerEXT(
        vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity =
                vk::DebugUtilsMessageSeverityFlagsEXT{
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError},
            .messageType =
                vk::DebugUtilsMessageTypeFlagsEXT{
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation},
            .pfnUserCallback = &debugCallback,
        });
  }

  void pickPhysicalDevice() {
    auto devices =
        m_instance.enumeratePhysicalDevices() |
        std::views::filter([](const vk::raii::PhysicalDevice& device) {
          return device.getProperties().apiVersion >= k_appInfo.apiVersion;
        }) |
        std::views::filter([](const vk::raii::PhysicalDevice& device) {
          return std::ranges::any_of(device.getQueueFamilyProperties(),
                                     [](const vk::QueueFamilyProperties& prop) {
                                       return (prop.queueFlags & k_queueFlag) !=
                                              vk::QueueFlagBits(0);
                                     });
        });
    if (devices.empty()) {
      throw std::runtime_error(std::format(
          "There is no device supporting API version:{} and Queue Family:{}",
          k_appInfo.apiVersion, vk::to_string(k_queueFlag)));
    }

    m_physicalDevice = std::move(devices.front());
  }

  vk::SharingMode createLogicalDevice() {
    constexpr uint32_t k_queueCount = 1;
    constexpr float k_queuePriority = 0.5f;
    constexpr std::array<const char*, 4> deviceExtensions = {
        vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName};
    const vk::StructureChain featureChain = {
        vk::PhysicalDeviceFeatures2{},
        vk::PhysicalDeviceVulkan11Features{
            .shaderDrawParameters = vk::True,
        },
        vk::PhysicalDeviceVulkan13Features{
            .synchronization2 = vk::True,
            .dynamicRendering = vk::True,
        },
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{
            .extendedDynamicState = vk::True,
        },
    };

    const auto queues = m_physicalDevice.getQueueFamilyProperties();
    auto graphicQueues =
        queues | std::views::enumerate | std::views::filter([](auto&& pair) {
          auto&& [idx, prop] = pair;
          return (prop.queueFlags & k_queueFlag) != vk::QueueFlagBits(0);
        }) |
        std::views::keys;
    m_graphicsIdx = graphicQueues.front();

    auto presentIdx = m_graphicsIdx;
    do {
      const auto& presentFilt = [this](size_t idx) {
        return m_physicalDevice.getSurfaceSupportKHR(idx, m_surface);
      };
      if (presentFilt(presentIdx)) {
        break;
      }

      auto presentGraphicsQueues =
          graphicQueues | std::views::filter(presentFilt) | std::views::take(1);
      if (!presentGraphicsQueues.empty()) {
        presentIdx = presentGraphicsQueues.front();
        break;
      }

      auto presentQueues = std::as_const(queues) | std::views::enumerate |
                           std::views::keys | std::views::filter(presentFilt) |
                           std::views::take(1);
      if (!presentQueues.empty()) {
        presentIdx = presentQueues.front();
        break;
      }

      throw std::runtime_error("Failed to find present queue!");
    } while (false);
    const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = m_graphicsIdx,
        .queueCount = k_queueCount,
        .pQueuePriorities = &k_queuePriority};

    m_device = vk::raii::Device(
        m_physicalDevice,
        vk::DeviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = deviceQueueCreateInfo.queueCount,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount =
                static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
        });
    m_graphicsQueue = vk::raii::Queue(m_device, m_graphicsIdx, 0);
    m_presentQueue = vk::raii::Queue(m_device, presentIdx, 0);
    return m_graphicsIdx != presentIdx ? vk::SharingMode::eConcurrent
                                       : vk::SharingMode::eExclusive;
  }

  void createSurface() {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) !=
        0) {
      throw std::runtime_error("Failed to create window surface!");
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, surface);
  }

  void createSwapChain() {
    auto capabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);

    m_swapChainExtent = capabilities.currentExtent;
    if (m_swapChainExtent.width ==
        std::numeric_limits<decltype(vk::Extent2D::width)>::max()) {
      static_assert(sizeof(decltype(m_swapChainExtent.width)) == sizeof(int));
      glfwGetFramebufferSize(m_window,
                             reinterpret_cast<int*>(&m_swapChainExtent.width),
                             reinterpret_cast<int*>(&m_swapChainExtent.height));
      m_swapChainExtent = vk::Extent2D(
          std::clamp(m_swapChainExtent.width, capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width),
          std::clamp(m_swapChainExtent.height,
                     capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height));
    }

    auto formats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
    auto preferredFormat =
        formats |
        std::views::filter([](const vk::SurfaceFormatKHR& surfaceFormat) {
          return surfaceFormat.format == vk::Format::eB8G8R8A8Srgb &&
                 surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        }) |
        std::views::take(1);
    const auto surfaceFomat =
        preferredFormat.empty() ? formats.front() : preferredFormat.front();

    auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface);
    auto preferredModes =
        presentModes | std::views::filter([](const vk::PresentModeKHR& mode) {
          return mode == vk::PresentModeKHR::eMailbox;
        }) |
        std::views::take(1);
    const auto presentMode = preferredModes.empty() ? vk::PresentModeKHR::eFifo
                                                    : preferredModes.front();
    m_swapChainImageFormat = surfaceFomat.format;

    m_swapChain = vk::raii::SwapchainKHR(
        m_device, vk::SwapchainCreateInfoKHR{
                      .flags = vk::SwapchainCreateFlagsKHR(),
                      .surface = m_surface,
                      .minImageCount = capabilities.minImageCount,
                      .imageFormat = m_swapChainImageFormat,
                      .imageColorSpace = surfaceFomat.colorSpace,
                      .imageExtent = m_swapChainExtent,
                      .imageArrayLayers = 1,
                      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                      .imageSharingMode = m_sharingMode,
                      .preTransform = capabilities.currentTransform,
                      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                      .presentMode = presentMode,
                      .clipped = true,
                      .oldSwapchain = nullptr,
                  });
    m_swapChainImages = m_swapChain.getImages();

    m_swapChainImageViews =
        m_swapChainImages |
        std::views::transform([this](const vk::Image& image) {
          return vk::raii::ImageView(
              m_device,
              vk::ImageViewCreateInfo{
                  .image = image,
                  .viewType = vk::ImageViewType::e2D,
                  .format = m_swapChainImageFormat,
                  .components =
                      vk::ComponentMapping{
                          .r = vk::ComponentSwizzle::eIdentity,
                          .g = vk::ComponentSwizzle::eIdentity,
                          .b = vk::ComponentSwizzle::eIdentity,
                          .a = vk::ComponentSwizzle::eIdentity,
                      },
                  .subresourceRange =
                      vk::ImageSubresourceRange{
                          .aspectMask = vk::ImageAspectFlagBits::eColor,
                          .baseMipLevel = 0,
                          .levelCount = 1,
                          .baseArrayLayer = 0,
                          .layerCount = 1,
                      },
              });
        }) |
        std::views::as_rvalue | std::ranges::to<std::vector>();
  }

  void createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboLayoutBinding(
        0, vk::DescriptorType::eUniformBuffer, 1,
        vk::ShaderStageFlagBits::eVertex, nullptr);
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .flags = {},
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding,
    };
    m_descriptorLayout = vk::raii::DescriptorSetLayout(m_device, layoutInfo);
  }

  void createGraphicsPipeline() {
    const std::filesystem::path spvFile = m_shadersPath / "22_shader_ubo.spv";
    std::ifstream shaderBin(spvFile, std::ios::ate | std::ios::binary);
    if (!shaderBin.is_open()) {
      throw std::runtime_error(
          std::format("Failed to open {}.", spvFile.string()));
    }
    std::vector<char> code(shaderBin.tellg());
    shaderBin.seekg(0, std::ios::beg);
    shaderBin.read(code.data(), static_cast<std::streamsize>(code.size()));
    shaderBin.close();

    vk::raii::ShaderModule module(
        m_device, vk::ShaderModuleCreateInfo{
                      .codeSize = code.size() * sizeof(char),
                      .pCode = reinterpret_cast<const uint32_t*>(code.data()),
                  });
    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = module,
            .pName = "vertMain",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = module,
            .pName = "fragMain",
        },
    };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = attributeDescriptions.size(),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
    };

    constexpr std::array k_dynamicStates = {vk::DynamicState::eViewport,
                                            vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(k_dynamicStates.size()),
        .pDynamicStates = k_dynamicStates.data(),
    };

    vk::PipelineViewportStateCreateInfo viewportState{
        .sType = vk::StructureType::ePipelineViewportStateCreateInfo,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    m_pipelineLayout = vk::raii::PipelineLayout(
        m_device, vk::PipelineLayoutCreateInfo{
                      .setLayoutCount = 1,
                      .pSetLayouts = &*m_descriptorLayout,
                      .pushConstantRangeCount = 0,
                  });

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_swapChainImageFormat,
    };
    m_graphicsPipeline =
        vk::raii::Pipeline(m_device, nullptr,
                           vk::GraphicsPipelineCreateInfo{
                               .pNext = &pipelineRenderingCreateInfo,
                               .stageCount = 2,
                               .pStages = shaderStages,
                               .pVertexInputState = &vertexInputInfo,
                               .pInputAssemblyState = &inputAssembly,
                               .pViewportState = &viewportState,
                               .pRasterizationState = &rasterizer,
                               .pMultisampleState = &multisampling,
                               .pColorBlendState = &colorBlending,
                               .pDynamicState = &dynamicState,
                               .layout = m_pipelineLayout,
                               .renderPass = nullptr,
                           });
  }

  template <class Buffer>
  vk::raii::DeviceMemory createDeviceMemory(const Buffer& buffer,
                                            vk::MemoryPropertyFlags property) {
    const vk::MemoryRequirements requirements = buffer.getMemoryRequirements();
    const vk::PhysicalDeviceMemoryProperties properties =
        m_physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
      if ((requirements.memoryTypeBits & (1 << i)) &&
          (properties.memoryTypes[i].propertyFlags & property) == property) {
        auto memory = vk::raii::DeviceMemory(
            m_device, vk::MemoryAllocateInfo{
                          .allocationSize = requirements.size,
                          .memoryTypeIndex = i,
                      });
        buffer.bindMemory(memory, 0);
        return std::move(memory);
      }
    }
    return nullptr;
  }

  void copyBuffer(vk::raii::Buffer& srcBuffer,
                  const vk::raii::Buffer& dstBuffer,
                  const vk::DeviceSize& size) {
    vk::raii::CommandBuffer copyCommandBuffer =
        std::move(m_device
                      .allocateCommandBuffers(vk::CommandBufferAllocateInfo{
                          .commandPool = m_commandPool,
                          .level = vk::CommandBufferLevel::ePrimary,
                          .commandBufferCount = 1,
                      })
                      .front());
    copyCommandBuffer.begin(vk::CommandBufferBeginInfo{});
    copyCommandBuffer.copyBuffer(srcBuffer, dstBuffer,
                                 vk::BufferCopy(0, 0, size));
    copyCommandBuffer.end();

    m_graphicsQueue.submit(
        vk::SubmitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*copyCommandBuffer,
        },
        nullptr);
    m_graphicsQueue.waitIdle();
  }

  void createVertexBuffer() {
    const vk::DeviceSize bufferSize =
        sizeof(k_vertices.at(0)) * k_vertices.size();

    auto stagingBuffer = vk::raii::Buffer(
        m_device, vk::BufferCreateInfo{
                      .size = bufferSize,
                      .usage = vk::BufferUsageFlagBits::eTransferSrc,
                      .sharingMode = vk::SharingMode::eExclusive,
                  });
    auto stagingBufferMemory = createDeviceMemory(
        stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible |
                           vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, k_vertices.data(), bufferSize);
    stagingBufferMemory.unmapMemory();

    m_vertexBuffer = vk::raii::Buffer(
        m_device, vk::BufferCreateInfo{
                      .size = bufferSize,
                      .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                               vk::BufferUsageFlagBits::eTransferDst,
                      .sharingMode = vk::SharingMode::eExclusive,
                  });
    m_vertexBufferMemory = createDeviceMemory(
        m_vertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
  }

  void createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(k_indices.at(0)) * k_indices.size();

    vk::raii::Buffer stagingBuffer(
        m_device, vk::BufferCreateInfo{
                      .size = bufferSize,
                      .usage = vk::BufferUsageFlagBits::eTransferSrc,
                      .sharingMode = vk::SharingMode::eExclusive,
                  });
    vk::raii::DeviceMemory stagingBufferMemory = createDeviceMemory(
        stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible |
                           vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = stagingBufferMemory.mapMemory(0, bufferSize);
    memcpy(data, k_indices.data(), (size_t)bufferSize);
    stagingBufferMemory.unmapMemory();

    m_indexBuffer = vk::raii::Buffer(
        m_device, vk::BufferCreateInfo{
                      .size = bufferSize,
                      .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                               vk::BufferUsageFlagBits::eTransferDst,
                      .sharingMode = vk::SharingMode::eExclusive,
                  });
    m_indexBufferMemory = createDeviceMemory(
        m_indexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
  }

  void createUniformBuffers() {
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();
    for (size_t i = 0; i < k_maxFramesInFlight; i++) {
      const vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
      vk::raii::Buffer buffer(
          m_device, vk::BufferCreateInfo{
                        .size = bufferSize,
                        .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                    });
      vk::raii::DeviceMemory bufferMem = createDeviceMemory(
          buffer, vk::MemoryPropertyFlagBits::eHostVisible |
                      vk::MemoryPropertyFlagBits::eHostCoherent);

      m_uniformBuffers.emplace_back(std::move(buffer));
      m_uniformBuffersMemory.emplace_back(std::move(bufferMem));
      m_uniformBuffersMapped.emplace_back(
          m_uniformBuffersMemory.at(i).mapMemory(0, bufferSize));
    }
  }

  void createDescriptorPool() {
    vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer,
                                    k_maxFramesInFlight);
    m_descriptorPool = vk::raii::DescriptorPool(
        m_device,
        vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = k_maxFramesInFlight,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        });
    std::vector<vk::DescriptorSetLayout> layouts(k_maxFramesInFlight,
                                                 *m_descriptorLayout);
    m_descriptorSets.clear();
    m_descriptorSets =
        m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        });
    for (size_t i = 0; i < k_maxFramesInFlight; i++) {
      vk::DescriptorBufferInfo bufferInfo{
          .buffer = m_uniformBuffers.at(i),
          .offset = 0,
          .range = sizeof(UniformBufferObject),
      };
      vk::WriteDescriptorSet descriptorWrite{
          .dstSet = m_descriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &bufferInfo,
      };
      m_device.updateDescriptorSets(descriptorWrite, {});
    }
  }

  void createCommandBuffers() {
    m_commandPool = vk::raii::CommandPool(
        m_device,
        vk::CommandPoolCreateInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = m_graphicsIdx,
        });
    m_commandBuffers = std::move(vk::raii::CommandBuffers(
        m_device, vk::CommandBufferAllocateInfo{
                      .commandPool = m_commandPool,
                      .level = vk::CommandBufferLevel::ePrimary,
                      .commandBufferCount = k_maxFramesInFlight,
                  }));
  }

  void recordCommandBuffer(vk::raii::CommandBuffer& commandBuffer,
                           uint32_t imageIdx) {
    commandBuffer.reset();

    const auto defaultBarrier = vk::ImageMemoryBarrier2{
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_swapChainImages.at(imageIdx),
        .subresourceRange =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    auto defaultDepenency = vk::DependencyInfo{
        .dependencyFlags = vk::DependencyFlags{},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = nullptr,
    };
    const vk::RenderingAttachmentInfo attachmentInfo = {
        .imageView = m_swapChainImageViews.at(imageIdx),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
    };

    commandBuffer.begin(vk::CommandBufferBeginInfo{});
    auto colorAttachmentBarrier = defaultBarrier;
    colorAttachmentBarrier.oldLayout = vk::ImageLayout::eUndefined;
    colorAttachmentBarrier.srcAccessMask = vk::AccessFlagBits2::eNone;
    colorAttachmentBarrier.srcStageMask =
        vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    colorAttachmentBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachmentBarrier.dstAccessMask =
        vk::AccessFlagBits2::eColorAttachmentWrite;
    colorAttachmentBarrier.dstStageMask =
        vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    defaultDepenency.pImageMemoryBarriers = &colorAttachmentBarrier;
    commandBuffer.pipelineBarrier2(defaultDepenency);

    commandBuffer.beginRendering(vk::RenderingInfo{
        .renderArea =
            {
                .offset = {0, 0},
                .extent = m_swapChainExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo,
    });

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               m_graphicsPipeline);
    commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
    commandBuffer.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint16);

    commandBuffer.setViewport(
        0,
        vk::Viewport(0.0f, 0.0f, static_cast<float>(m_swapChainExtent.width),
                     static_cast<float>(m_swapChainExtent.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0,
                             vk::Rect2D(vk::Offset2D(0, 0), m_swapChainExtent));

    // commandBuffer.draw(k_vertices.size(), k_vertices.size() / 3, 0, 0);
    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0,
        *m_descriptorSets.at(imageIdx % k_maxFramesInFlight), nullptr);
    commandBuffer.drawIndexed(k_indices.size(), 1, 0, 0, 0);
    commandBuffer.endRendering();

    auto presentBarrier = defaultBarrier;
    presentBarrier.oldLayout = colorAttachmentBarrier.newLayout;
    presentBarrier.srcAccessMask = colorAttachmentBarrier.dstAccessMask;
    presentBarrier.srcStageMask = colorAttachmentBarrier.dstStageMask;
    presentBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    presentBarrier.dstAccessMask = colorAttachmentBarrier.srcAccessMask;
    presentBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
    defaultDepenency.pImageMemoryBarriers = &presentBarrier;
    commandBuffer.pipelineBarrier2(defaultDepenency);
    commandBuffer.end();
  }

  void createSyncObjects() {
    for (const auto& _ : m_swapChainImages) {
      m_renderFinishedSemaphores.emplace_back(m_device,
                                              vk::SemaphoreCreateInfo());
    }
    for (auto i = 0; i < k_maxFramesInFlight; i++) {
      m_presentCompletedSemaphores.emplace_back(m_device,
                                                vk::SemaphoreCreateInfo());
      m_drawFences.emplace_back(
          m_device,
          vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      drawFrame();
    }
  }

  void drawFrame() {
    auto fence = *m_drawFences.at(m_frameIdx);
    if (m_device.waitForFences(fence, vk::True,
                               std::numeric_limits<uint64_t>::max()) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("Failed to wait for fences.");
    }
    m_device.resetFences(fence);

    auto presentSem = *m_presentCompletedSemaphores.at(m_frameIdx);
    auto [result, imageIdx] = m_swapChain.acquireNextImage(
        std::numeric_limits<uint64_t>::max(), presentSem, nullptr);
    switch (result) {
      case vk::Result::eSuccess:
        break;
      case vk::Result::eSuboptimalKHR:
      case vk::Result::eErrorOutOfDateKHR:
        recreateSwapChain();
        return;
      default:
        throw std::runtime_error(std::format(
            "Failed to acquire next image, error:{}", vk::to_string(result)));
    }

    updateUniformBuffer(imageIdx);

    auto& commandBuffer = m_commandBuffers.at(m_frameIdx);
    recordCommandBuffer(commandBuffer, imageIdx);

    const auto renderSem = *m_renderFinishedSemaphores.at(imageIdx);
    const vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    m_graphicsQueue.submit(
        vk::SubmitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &presentSem,
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderSem,
        },
        fence);

    if (m_presentQueue.presentKHR(vk::PresentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderSem,
            .swapchainCount = 1,
            .pSwapchains = &*m_swapChain,
            .pImageIndices = &imageIdx,
        }) != vk::Result::eSuccess) {
      throw std::runtime_error("Failed to present image.");
    }
    if (m_frameBufferResized) {
      m_frameBufferResized = false;
      recreateSwapChain();
    }

    m_frameIdx = (m_frameIdx + 1) % k_maxFramesInFlight;
  }

  void updateUniformBuffer(uint32_t imageIdx) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj =
        glm::perspective(glm::radians(45.0f),
                         static_cast<float>(m_swapChainExtent.width) /
                             static_cast<float>(m_swapChainExtent.height),
                         0.1f, 10.0f);
    ubo.proj[1][1] *= -1;
    memcpy(m_uniformBuffersMapped.at(imageIdx % k_maxFramesInFlight), &ubo,
           sizeof(ubo));
  }

  void createTextureImage() {
    int texWidth, texHeight, texChannels;
    const std::filesystem::path textureFile = m_texturesPath / "texture.jpg";
    stbi_uc* pixels = stbi_load(textureFile.c_str(), &texWidth, &texHeight,
                                &texChannels, STBI_rgb_alpha);
    if (!pixels) {
      throw std::runtime_error(
          std::format("Failed to open {}.", textureFile.string()));
    }

    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    auto stagingBuffer = vk::raii::Buffer(
        m_device, vk::BufferCreateInfo{
                      .size = imageSize,
                      .usage = vk::BufferUsageFlagBits::eTransferSrc,
                      .sharingMode = vk::SharingMode::eExclusive,
                  });
    auto stagingBufferMemory = createDeviceMemory(
        stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible |
                           vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = stagingBufferMemory.mapMemory(0, imageSize);
    memcpy(data, pixels, imageSize);
    stagingBufferMemory.unmapMemory();

    stbi_image_free(pixels);

    m_textureImage = vk::raii::Image(
        m_device, vk::ImageCreateInfo{
                      .imageType = vk::ImageType::e2D,
                      .format = vk::Format::eR8G8B8A8Srgb,
                      .extent =
                          vk::Extent3D{
                              .width = static_cast<uint32_t>(texWidth),
                              .height = static_cast<uint32_t>(texHeight),
                              .depth = 1,
                          },
                      .mipLevels = 1,
                      .arrayLayers = 1,
                      .samples = vk::SampleCountFlagBits::e1,
                      .tiling = vk::ImageTiling::eOptimal,
                      .usage = vk::ImageUsageFlagBits::eTransferDst |
                               vk::ImageUsageFlagBits::eSampled,
                      .sharingMode = vk::SharingMode::eExclusive,
                      .initialLayout = vk::ImageLayout::eUndefined,
                  });
    m_textureImageMemory = createDeviceMemory(
        m_textureImage, vk::MemoryPropertyFlagBits::eDeviceLocal);
  }

  void cleanup() {
    m_device.waitIdle();
    cleanupSwapChain();
    glfwDestroyWindow(m_window);
    glfwTerminate();
  }

  void cleanupSwapChain() {
    m_swapChainImageViews.clear();
    m_swapChain = nullptr;
  }

  void recreateSwapChain() {
    m_device.waitIdle();
    cleanupSwapChain();
    createSwapChain();
  }

 public:
  bool m_frameBufferResized = false;

 private:
  std::filesystem::path m_texturesPath;
  std::filesystem::path m_shadersPath;

  GLFWwindow* m_window;
  vk::raii::Context m_context;
  vk::raii::Instance m_instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT m_debugger = nullptr;

  vk::raii::SurfaceKHR m_surface = nullptr;
  vk::raii::PhysicalDevice m_physicalDevice = nullptr;
  vk::raii::Device m_device = nullptr;
  uint32_t m_graphicsIdx = -1;
  vk::raii::Queue m_graphicsQueue = nullptr;
  vk::raii::Queue m_presentQueue = nullptr;
  vk::SharingMode m_sharingMode;

  vk::raii::SwapchainKHR m_swapChain = nullptr;
  std::vector<vk::Image> m_swapChainImages;
  vk::Extent2D m_swapChainExtent{};
  vk::Format m_swapChainImageFormat = vk::Format::eUndefined;
  std::vector<vk::raii::ImageView> m_swapChainImageViews;

  vk::raii::DescriptorSetLayout m_descriptorLayout = nullptr;
  vk::raii::PipelineLayout m_pipelineLayout = nullptr;
  vk::raii::Pipeline m_graphicsPipeline = nullptr;

  vk::raii::Buffer m_vertexBuffer = nullptr;
  vk::raii::DeviceMemory m_vertexBufferMemory = nullptr;
  vk::raii::Buffer m_indexBuffer = nullptr;
  vk::raii::DeviceMemory m_indexBufferMemory = nullptr;

  vk::raii::DescriptorPool m_descriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> m_descriptorSets;
  std::vector<vk::raii::Buffer> m_uniformBuffers;
  std::vector<vk::raii::DeviceMemory> m_uniformBuffersMemory;
  std::vector<void*> m_uniformBuffersMapped;

  vk::raii::CommandPool m_commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> m_commandBuffers;

  static constexpr uint32_t k_maxFramesInFlight = 4;
  uint32_t m_frameIdx = 0;
  std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
  std::vector<vk::raii::Semaphore> m_presentCompletedSemaphores;
  std::vector<vk::raii::Fence> m_drawFences;

  vk::raii::Image m_textureImage = nullptr;
  vk::raii::DeviceMemory m_textureImageMemory = nullptr;
};

int main() {
  auto app = std::make_unique<HelloTriangleApplication>();

  try {
    app->run();
  } catch (const std::exception& e) {
    std::println("{}", e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
