#include "base.cpp"

static constexpr vk::QueueFlagBits k_queueFlag = vk::QueueFlagBits::eGraphics;
static constexpr std::array k_validationLayers = {
#ifndef NDEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
};
static constexpr vk::ApplicationInfo k_appInfo{
    .pApplicationName = Application::k_name.data(),
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "No Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = vk::ApiVersion14,
};
static constexpr std::array k_deviceExtensions = {
    vk::KHRSwapchainExtensionName,
    vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::KHRCreateRenderpass2ExtensionName,
};
static const vk::StructureChain k_featureChain = {
    vk::PhysicalDeviceFeatures2{.features = {.samplerAnisotropy = vk::True}},
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
static constexpr float k_queuePriority = 0.5f;

template <std::ranges::input_range RequiredRange,
          std::ranges::input_range SupportedRange, typename Proj>
static bool arePropertiesSupported(const RequiredRange& required,
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

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
  if (type != vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
    return vk::True;
  }

  switch (severity) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
      std::println("[VULKAN-VALIDATION] VERBOSE: type {}, msg: {}",
                   vk::to_string(type), pCallbackData->pMessage);
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
      std::println("[VULKAN-VALIDATION] INFO: type {}, msg: {}",
                   vk::to_string(type), pCallbackData->pMessage);
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
      std::println("[VULKAN-VALIDATION] WARN: type {}, msg: {}",
                   vk::to_string(type), pCallbackData->pMessage);
      break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
      std::println("[VULKAN-VALIDATION] ERROR: type {}, msg: {}",
                   vk::to_string(type), pCallbackData->pMessage);
      break;
  }
  return vk::False;
}

constexpr bool isDebugLayerEnabled() { return !k_validationLayers.empty(); }

RenderSystem::RenderSystem(Window& window) {
  std::vector<const char*> layers{};
  std::vector<const char*> extensions = window.getExtensions();
  if (isDebugLayerEnabled()) {
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
    layers.assign(k_validationLayers.begin(), k_validationLayers.end());
  }
  vk::raii::Context context;
  if (!arePropertiesSupported(layers,
                              context.enumerateInstanceLayerProperties(),
                              &vk::LayerProperties::layerName)) {
    throw std::runtime_error("One or more required layers are not supported!");
  }
  if (!arePropertiesSupported(extensions,
                              context.enumerateInstanceExtensionProperties(),
                              &vk::ExtensionProperties::extensionName)) {
    throw std::runtime_error(
        "One or more required extension are not supported!");
  }
  vk::raii::Instance instance = vk::raii::Instance(
      context,
      vk::InstanceCreateInfo{
          .pApplicationInfo = &k_appInfo,
          .enabledLayerCount = static_cast<uint32_t>(layers.size()),
          .ppEnabledLayerNames = layers.data(),
          .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
          .ppEnabledExtensionNames = extensions.data(),
      });

  vk::raii::DebugUtilsMessengerEXT debugger =
      isDebugLayerEnabled()
          ? instance.createDebugUtilsMessengerEXT(
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
                })
          : nullptr;
  auto devices =
      instance.enumeratePhysicalDevices() |
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
  auto physicalDevice = devices.front();

  const auto queues = devices.front().getQueueFamilyProperties();
  auto graphicQueues =
      queues | std::views::enumerate | std::views::filter([](auto&& pair) {
        auto&& [idx, prop] = pair;
        return (prop.queueFlags & k_queueFlag) != vk::QueueFlagBits(0);
      }) |
      std::views::keys;
  uint32_t graphicsIdx = graphicQueues.front();

  auto surface = window.createSurface(*instance);
  auto presentIdx = graphicsIdx;
  do {
    const auto& presentFilt = [&physicalDevice, &surface](size_t idx) {
      return physicalDevice.getSurfaceSupportKHR(idx, surface);
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

  auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  auto currentExtent = capabilities.currentExtent;
  if (currentExtent.width ==
      std::numeric_limits<decltype(vk::Extent2D::width)>::max()) {
    static_assert(sizeof(decltype(currentExtent.width)) == sizeof(int));
    currentExtent = window.getExtent();
    currentExtent = vk::Extent2D(
        std::clamp(currentExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width),
        std::clamp(currentExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height));
  }

  auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
  auto preferredFormat =
      formats |
      std::views::filter([](const vk::SurfaceFormatKHR& surfaceFormat) {
        return surfaceFormat.format == vk::Format::eB8G8R8A8Srgb &&
               surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      }) |
      std::views::take(1);

  auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
  auto preferredModes = presentModes |
                        std::views::filter([](const vk::PresentModeKHR& mode) {
                          return mode == vk::PresentModeKHR::eMailbox;
                        }) |
                        std::views::take(1);

  const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = graphicsIdx,
      .queueCount = 1,
      .pQueuePriorities = &k_queuePriority,
  };
  auto logicalDevice = vk::raii::Device(
      physicalDevice,
      vk::DeviceCreateInfo{
          .pNext = &k_featureChain.get<vk::PhysicalDeviceFeatures2>(),
          .queueCreateInfoCount = deviceQueueCreateInfo.queueCount,
          .pQueueCreateInfos = &deviceQueueCreateInfo,
          .enabledExtensionCount =
              static_cast<uint32_t>(k_deviceExtensions.size()),
          .ppEnabledExtensionNames = k_deviceExtensions.data(),
      });

  m_renderer = std::make_unique<Renderer>(
      surface, std::move(context), std::move(instance), std::move(debugger),
      std::move(physicalDevice), std::move(logicalDevice), graphicsIdx,
      presentIdx, std::move(capabilities),
      preferredFormat.empty() ? formats.front() : preferredFormat.front(),
      currentExtent,
      preferredModes.empty() ? vk::PresentModeKHR::eFifo
                             : preferredModes.front());
}

FrameData RenderSystem::PreDraw() { return m_renderer->PreSubmit(); }

void RenderSystem::Draw(FrameData& frameData) {
  frameData.bufferBuilder->Build();
  frameData.vertexBuilder->Build();
  m_renderer->Submit(frameData);
}

void RenderSystem::PostDraw(FrameData& frameData) {
  m_renderer->PostSubmit(frameData);
}

std::shared_ptr<Renderer> RenderSystem::GetRenderer() { return m_renderer; }

Renderer::Renderer(VkSurfaceKHR surface, vk::raii::Context context,
                   vk::raii::Instance instance,
                   vk::raii::DebugUtilsMessengerEXT debugger,
                   vk::raii::PhysicalDevice physicalDevice,
                   vk::raii::Device logicalDevice, uint32_t graphicsQueueIdx,
                   uint32_t presentQueueIdx,
                   vk::SurfaceCapabilitiesKHR capabilities,
                   vk::SurfaceFormatKHR surfaceFormat,
                   vk::Extent2D swapchainExtent, vk::PresentModeKHR presentMode)
    : m_context(std::move(context)),
      m_instance(std::move(instance)),
      m_debugger(std::move(debugger)),
      m_surface(vk::raii::SurfaceKHR(m_instance, surface)),
      m_physicalDevice(std::move(physicalDevice)),
      m_device(std::move(logicalDevice)),
      m_graphicsQueue(vk::raii::Queue(m_device, graphicsQueueIdx, 0)),
      m_presentQueue(vk::raii::Queue(m_device, presentQueueIdx, 0)),
      m_swapChainExtent(swapchainExtent),
      m_swapChain(vk::raii::SwapchainKHR(
          m_device,
          vk::SwapchainCreateInfoKHR{
              .flags = vk::SwapchainCreateFlagsKHR(),
              .surface = surface,
              .minImageCount = capabilities.minImageCount,
              .imageFormat = surfaceFormat.format,
              .imageColorSpace = surfaceFormat.colorSpace,
              .imageExtent = m_swapChainExtent,
              .imageArrayLayers = 1,
              .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
              .imageSharingMode = graphicsQueueIdx != presentQueueIdx
                                      ? vk::SharingMode::eConcurrent
                                      : vk::SharingMode::eExclusive,
              .preTransform = capabilities.currentTransform,
              .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
              .presentMode = presentMode,
              .clipped = true,
              .oldSwapchain = nullptr,
          })),
      m_swapChainImages(m_swapChain.getImages()),
      m_swapChainImageViews(
          m_swapChainImages |
          std::views::transform(
              [format = surfaceFormat.format, this](const vk::Image& image) {
                return vk::raii::ImageView(
                    m_device,
                    vk::ImageViewCreateInfo{
                        .image = image,
                        .viewType = vk::ImageViewType::e2D,
                        .format = format,
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
          std::ranges::to<std::vector>()),
      m_renderFinishedSemaphores(
          std::views::transform(m_swapChainImages,
                                [this](const auto&) {
                                  return vk::raii::Semaphore(
                                      m_device, vk::SemaphoreCreateInfo());
                                }) |
          std::ranges::to<std::vector>()),
      m_presentCompletedSemaphore(m_device, vk::SemaphoreCreateInfo{}),
      m_drawFence(
          m_device,
          vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}),
      m_shadersFactory(std::make_shared<ShaderFactory>(
          m_physicalDevice, m_device, graphicsQueueIdx)),
      m_buffersFactory(std::make_shared<BufferFactory>(
          m_physicalDevice, m_device, graphicsQueueIdx)),
      m_stagingBuffersFactory(std::make_shared<BufferFactory>(
          m_physicalDevice, m_device, graphicsQueueIdx)),
      m_pipelinesFactory(std::make_unique<PipelineFactory>(m_device)),
      m_commandBuffer(
          std::move(m_buffersFactory->NewCommandBuffers(1).front())) {}

FrameData Renderer::PreSubmit() {
  if (m_device.waitForFences(*m_drawFence, vk::True,
                             std::numeric_limits<uint64_t>::max()) !=
      vk::Result::eSuccess) {
    throw std::runtime_error("Failed to wait for fences.");
  }
  m_device.resetFences(*m_drawFence);

  FrameData frameData = {
      .commandBuffer = m_commandBuffer,
      .shaderBuilder = m_shadersFactory->NewBuilder(),
      .bufferBuilder = m_stagingBuffersFactory->NewStagingBuilder(),
      .vertexBuilder = m_buffersFactory->NewBuilder(),
  };
  auto [result, imageIndex] =
      m_swapChain.acquireNextImage(std::numeric_limits<uint64_t>::max(),
                                   *m_presentCompletedSemaphore, nullptr);
  switch (result) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorOutOfDateKHR:
      break;
    default:
      throw std::runtime_error(std::format(
          "Failed to acquire next image, error:{}", vk::to_string(result)));
  }
  frameData.imageIndex = imageIndex;
  frameData.commandBuffer.reset();
  frameData.commandBuffer.begin(vk::CommandBufferBeginInfo{});
  return frameData;
}

void Renderer::Submit(FrameData& frameData) {
  auto& srcBuffer =
      m_stagingBuffersFactory->Buffer(frameData.bufferBuilder->ID());
  auto& destBuffer = m_buffersFactory->Buffer(frameData.vertexBuilder->ID());
  frameData.commandBuffer.copyBuffer(
      srcBuffer, destBuffer,
      vk::BufferCopy(0, 0, vk::DeviceSize(frameData.vertexBuilder->Size())));

  transitionImageLayout(
      frameData.commandBuffer, m_swapChainImages.at(frameData.imageIndex),
      vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
  const vk::RenderingAttachmentInfo colorAttachmentInfo = {
      .imageView = m_swapChainImageViews.at(frameData.imageIndex),
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = vk::ClearColorValue(0.f, 0.f, 0.f, 1.f),
  };
  frameData.commandBuffer.beginRendering(vk::RenderingInfo{
      .renderArea =
          {
              .offset = {0, 0},
              .extent = m_swapChainExtent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentInfo,
      .pDepthAttachment = nullptr,
  });
  frameData.commandBuffer.setViewport(
      0,
      vk::Viewport(0.0f, 0.0f, static_cast<float>(m_swapChainExtent.width),
                   static_cast<float>(m_swapChainExtent.height), 0.0f, 1.0f));
  frameData.commandBuffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), m_swapChainExtent));
  // commandBuffer.bindPipeline(
  //     vk::PipelineBindPoint::eGraphics,
  //     m_pipelinesAlloc->getPipeline(*m_program, PipelineState{}));
  // commandBuffer.bindVertexBuffers(
  //     0,
  //     *m_buffersAlloc->buffer(std::to_underlying(BufferID::TriangleVertex)),
  //     {0});
  // commandBuffer.bindIndexBuffer(
  //     *m_buffersAlloc->buffer(std::to_underlying(BufferID::TriangleIndex)),
  //     0, vk::IndexType::eUint32);
  // commandBuffer.bindDescriptorSets(
  //     vk::PipelineBindPoint::eGraphics, m_pipelines->layout(), 0,
  //     *m_resources->descriptorSet(std::to_underlying(BufferID::TriangleIndex),
  //                                 imageIdx % Window::k_maxFramesInFlight),
  //     nullptr);
  // commandBuffer.drawIndexed(3, 1, 0, 0, 0);
  frameData.commandBuffer.endRendering();
  transitionImageLayout(m_commandBuffer,
                        m_swapChainImages.at(frameData.imageIndex),
                        vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR);
  frameData.commandBuffer.end();

  const vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  m_graphicsQueue.submit(
      vk::SubmitInfo{
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &*m_presentCompletedSemaphore,
          .pWaitDstStageMask = &waitDestinationStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &*frameData.commandBuffer,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores =
              &*m_renderFinishedSemaphores.at(frameData.imageIndex),
      },
      m_drawFence);
}

void Renderer::PostSubmit(FrameData& frameData) {
  if (m_presentQueue.presentKHR(vk::PresentInfoKHR{
          .waitSemaphoreCount = 1,
          .pWaitSemaphores =
              &*m_renderFinishedSemaphores.at(frameData.imageIndex),
          .swapchainCount = 1,
          .pSwapchains = &*m_swapChain,
          .pImageIndices = &frameData.imageIndex,
      }) != vk::Result::eSuccess) {
    throw std::runtime_error("Failed to present image.");
  }
}

void Renderer::transitionImageLayout(
    const vk::raii::CommandBuffer& commandBuffer, const vk::Image& image,
    vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
  vk::ImageMemoryBarrier2 barrier;
  barrier.setOldLayout(oldLayout)
      .setNewLayout(newLayout)
      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setImage(image)
      .setSubresourceRange(vk::ImageSubresourceRange()
                               .setAspectMask(vk::ImageAspectFlagBits::eColor)
                               .setBaseMipLevel(0)
                               .setLevelCount(1)
                               .setBaseArrayLayer(0)
                               .setLayerCount(1));

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlags2{})
        .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
        .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer);
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderRead)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader);
  } else if (oldLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  } else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
             newLayout == vk::ImageLayout::ePresentSrcKHR) {
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eNone)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
  } else if (oldLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::eDepthAttachmentOptimal) {
    barrier
        .setSubresourceRange(barrier.subresourceRange.setAspectMask(
            vk::ImageAspectFlagBits::eDepth))
        .setSrcAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                         vk::PipelineStageFlagBits2::eLateFragmentTests)
        .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                         vk::PipelineStageFlagBits2::eLateFragmentTests);
  } else if (oldLayout == vk::ImageLayout::eUndefined &&
             newLayout == vk::ImageLayout::ePresentSrcKHR) {
    barrier.setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setDstAccessMask(vk::AccessFlagBits2::eNone)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
        .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe);
  } else {
    throw std::invalid_argument("Unsupported layout transition!");
  }

  commandBuffer.pipelineBarrier2(vk::DependencyInfo{
      .dependencyFlags = vk::DependencyFlags{},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,

  });
}

template <class T>
size_t Allocator<T>::add(T&& data) {
  auto it = m_table.emplace(std::hash<T>{}(data), m_data.size());
  m_data.emplace_back(std::move(data));
  return it.first->first;
}

template <class T>
template <class... Args>
size_t Allocator<T>::add(std::tuple<const Args&...> args, T&& data) {
  auto it = m_table.emplace(std::hash<std::tuple<const Args&...>>{}(args),
                            m_data.size());
  m_data.emplace_back(std::move(data));
  return it.first->first;
}

template <class T>
template <class... Args>
size_t Allocator<T>::add(std::tuple<Args...> args, T&& data) {
  auto it =
      m_table.emplace(std::hash<std::tuple<Args...>>{}(args), m_data.size());
  m_data.emplace_back(std::move(data));
  return it.first->first;
}

template <class T>
T& Allocator<T>::operator[](size_t id) {
  return m_data[m_table[id]];
}

struct ShaderReflection {
  struct Binding {
    std::string name;
    std::string kind;
    uint32_t index;
    bool operator==(const Binding& other) const {
      return name == other.name && kind == other.kind && index == other.index;
    }
  };
  struct ElementType {
    std::string kind;
    std::string baseShape;
    std::string access;
    std::string name;

    uint32_t elementCount = 1;
    std::unique_ptr<ElementType> elementType;
  };
  struct Field {
    ElementType type;
    Binding binding;
  };
  struct Parameter {
    struct Type {
      struct ElementType {
        std::vector<Field> fields;
      } elementType;
    } type;
    Binding binding;
  };
  std::vector<Parameter> parameters;

  struct EntryPoint {
    std::string name;
    std::string stage;
    std::vector<Binding> bindings;
  };
  std::vector<EntryPoint> entryPoints;
};

static vk::DescriptorType convertFrom(
    const ShaderReflection::ElementType& type) {
  if (type.kind == "constantBuffer") {
    return vk::DescriptorType::eUniformBuffer;
  } else if (type.kind == "samplerState") {
    return vk::DescriptorType::eSampler;
  } else if (type.kind == "resource") {
    if (type.baseShape == "texture2D") {
      return type.access == "readWrite" ? vk::DescriptorType::eStorageImage
                                        : vk::DescriptorType::eSampledImage;
    } else if (type.baseShape == "structuredBuffer") {
      return vk::DescriptorType::eStorageBuffer;
    } else if (type.baseShape == "textureBuffer") {
      return type.access == "readWrite"
                 ? vk::DescriptorType::eStorageTexelBuffer
                 : vk::DescriptorType::eUniformTexelBuffer;
    } else if (type.baseShape == "accelerationStructure") {
      return vk::DescriptorType::eAccelerationStructureKHR;
    }
  } else if (type.kind == "struct") {
    if (type.name == "__SubpassImpl") {
      return vk::DescriptorType::eInputAttachment;
    }
  } else if (type.kind == "array") {
    return convertFrom(*type.elementType);
  }
  throw std::runtime_error(
      std::format("Unsupported descriptor type:{}", type.kind));
}

static vk::ShaderStageFlagBits convertFrom(
    const ShaderReflection::EntryPoint& entryPoint) {
  if (entryPoint.stage == "vertex") {
    return vk::ShaderStageFlagBits::eVertex;
  } else if (entryPoint.stage == "fragment") {
    return vk::ShaderStageFlagBits::eFragment;
  }
  throw std::runtime_error(
      std::format("Unsupported shader stage:{}", entryPoint.stage));
}

bool Vertex::operator==(const Vertex& other) const {
  return other.pos == pos && other.color == color;
};

BufferFactory::BufferFactory(vk::raii::PhysicalDevice& physicalDevice,
                             vk::raii::Device& device, uint32_t queueIndex)
    : m_physicalDevice(physicalDevice),
      m_device(device),
      m_commandPool(
          device,
          vk::CommandPoolCreateInfo{
              .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
              .queueFamilyIndex = queueIndex,
          }) {}

template <class T>
size_t BufferFactory::NewStagingBuffer(std::span<T> data) {
  const vk::DeviceSize bufferSize = sizeof(T) * data.size();
  auto id =
      add(std::tie(data),
          vk::raii::Buffer(m_device,
                           vk::BufferCreateInfo{
                               .size = bufferSize,
                               .usage = vk::BufferUsageFlagBits::eTransferSrc,
                               .sharingMode = vk::SharingMode::eExclusive,
                           }));
  m_memories.emplace_back(createDeviceMemory(
      m_data.back(), vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent));
  void* buffer = m_memories.back().mapMemory(0, bufferSize);
  memcpy(buffer, data.data(), bufferSize);
  m_memories.back().unmapMemory();
  return id;
}

template <class T>
size_t BufferFactory::NewVertexBuffer(std::span<T> data) {
  const vk::DeviceSize bufferSize = sizeof(T) * data.size();
  auto id =
      add(std::tie(data),
          vk::raii::Buffer(m_device,
                           vk::BufferCreateInfo{
                               .size = bufferSize,
                               .usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                        vk::BufferUsageFlagBits::eTransferDst,
                               .sharingMode = vk::SharingMode::eExclusive,
                           }));
  m_memories.emplace_back(createDeviceMemory(
      m_data.back(), vk::MemoryPropertyFlagBits::eDeviceLocal));
  return id;
}

vk::raii::Buffer& BufferFactory::Buffer(size_t id) { return operator[](id); }

std::unique_ptr<StagingBufferBuilder> BufferFactory::NewStagingBuilder() {
  return std::make_unique<StagingBufferBuilder>(weak_from_this());
}

std::unique_ptr<VertexBuilder> BufferFactory::NewBuilder() {
  return std::make_unique<VertexBuilder>(weak_from_this());
}

vk::raii::CommandBuffers BufferFactory::NewCommandBuffers(uint32_t count) {
  return vk::raii::CommandBuffers(m_device,
                                  vk::CommandBufferAllocateInfo{
                                      .commandPool = m_commandPool,
                                      .level = vk::CommandBufferLevel::ePrimary,
                                      .commandBufferCount = count,
                                  });
}

vk::raii::DeviceMemory BufferFactory::createDeviceMemory(
    const vk::raii::Buffer& buffer, vk::MemoryPropertyFlags property) {
  const vk::MemoryRequirements requirements = buffer.getMemoryRequirements();
  auto memoryTypeIndex =
      m_physicalDevice.getMemoryProperties().memoryTypes |
      std::views::enumerate |
      std::views::filter(
          [this, property,
           memoryTypeBits = requirements.memoryTypeBits](auto&& pair) {
            auto&& [idx, memoryType] = pair;
            return (memoryTypeBits & (1 << idx)) &&
                   (memoryType.propertyFlags & property) == property;
          }) |
      std::views::take(1) | std::views::keys;
  if (memoryTypeIndex.empty()) return nullptr;

  auto memory = vk::raii::DeviceMemory(
      m_device,
      vk::MemoryAllocateInfo{
          .allocationSize = requirements.size,
          .memoryTypeIndex = static_cast<uint32_t>(memoryTypeIndex.front()),
      });
  buffer.bindMemory(memory, 0);
  return std::move(memory);
}

VertexBuilder::VertexBuilder(std::weak_ptr<BufferFactory> factory)
    : m_factory(factory) {}

VertexBuilder& VertexBuilder::SetVertices(const std::vector<Vertex>& vertices) {
  m_vertices = vertices;
  return *this;
}

size_t VertexBuilder::Build() {
  if (auto factory = m_factory.lock()) {
    m_size = sizeof(m_vertices.at(0)) * m_vertices.size();
    m_id = factory->NewVertexBuffer(
        std::span(m_vertices.data(), m_vertices.size()));
  }
  return m_id;
}

uint64_t VertexBuilder::Size() { return m_size; }

size_t VertexBuilder::ID() { return m_id; }

StagingBufferBuilder::StagingBufferBuilder(std::weak_ptr<BufferFactory> factory)
    : m_factory(factory) {}

StagingBufferBuilder& StagingBufferBuilder::SetVertices(
    const std::vector<Vertex>& vertices) {
  m_vertices = vertices;
  return *this;
}

size_t StagingBufferBuilder::Build() {
  if (auto factory = m_factory.lock()) {
    m_id = factory->NewStagingBuffer(
        std::span(m_vertices.data(), m_vertices.size()));
  }
  return m_id;
}

size_t StagingBufferBuilder::ID() { return m_id; }

ShaderProgram::ShaderProgram(const vk::raii::Device& device,
                             const std::filesystem::path& spvFile,
                             const std::filesystem::path& reflectionFile)
    : m_pool(nullptr), m_program(nullptr) {
  std::ifstream file(reflectionFile);
  if (!file.is_open()) {
    throw std::runtime_error(
        std::format("Failed to open {}", reflectionFile.string()));
  }
  glz::basic_istream_buffer buffer(file);
  ShaderReflection reflection;
  if (auto ec = glz::read<glz::opts{
          .error_on_unknown_keys = false,
      }>(reflection, buffer)) {
    throw std::runtime_error(std::format("Failed to parse {}, ec: {}",
                                         reflectionFile.string(),
                                         std::to_underlying(ec.ec)));
  };
  file.close();

  file = std::ifstream(spvFile, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error(
        std::format("Failed to open {}.", spvFile.string()));
  }
  std::vector<char> code(file.tellg());
  file.seekg(0, std::ios::beg);
  file.read(code.data(), static_cast<std::streamsize>(code.size()));
  file.close();

  m_hash = fnv1a(code);
  m_program = vk::raii::ShaderModule(
      device, vk::ShaderModuleCreateInfo{
                  .codeSize = code.size() * sizeof(char),
                  .pCode = reinterpret_cast<const uint32_t*>(code.data()),
              });
  m_stages = reflection.entryPoints |
             std::views::transform(
                 [this](const ShaderReflection::EntryPoint& entryPoint) {
                   return std::pair(convertFrom(entryPoint), entryPoint.name);
                 }) |
             std::ranges::to<decltype(m_stages)>();
  m_poolTypes =
      reflection.parameters |
      std::views::transform([](const ShaderReflection::Parameter& param) {
        return param.type.elementType.fields | std::views::all;
      }) |
      std::views::join |
      std::views::transform([](const ShaderReflection::Field& field) {
        return std::pair(convertFrom(field.type), 0);
      }) |
      std::ranges::to<decltype(m_poolTypes)>();
  m_layouts =
      reflection.parameters |
      std::views::transform([this, &device, &reflection](
                                const ShaderReflection::Parameter& param) {
        auto bindings =
            param.type.elementType.fields |
            std::views::transform([this, &param, &device, &reflection](
                                      const ShaderReflection::Field& field) {
              return vk::DescriptorSetLayoutBinding(
                  field.binding.index, convertFrom(field.type),
                  field.type.elementCount,
                  std::ranges::fold_left(
                      reflection.entryPoints |
                          std::views::filter(
                              [&param](const ShaderReflection::EntryPoint&
                                           entryPoint) {
                                return std::ranges::any_of(
                                    entryPoint.bindings,
                                    [&param](const ShaderReflection::Binding&
                                                 binding) {
                                      return binding == param.binding;
                                    });
                              }) |
                          std::views::transform(
                              [](const ShaderReflection::EntryPoint&
                                     entryPoint) {
                                return convertFrom(entryPoint);
                              }),
                      vk::ShaderStageFlagBits::eVertex,
                      std::bit_xor<vk::ShaderStageFlags>()),
                  nullptr);
            }) |
            std::ranges::to<std::vector>();
        return vk::raii::DescriptorSetLayout(
            device, vk::DescriptorSetLayoutCreateInfo({
                        .bindingCount = static_cast<uint32_t>(bindings.size()),
                        .pBindings = bindings.data(),
                    }));
      }) |
      std::ranges::to<std::vector>();
  auto layouts = getLayouts();
  // m_descriptorSets =
  //     m_device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
  //         .descriptorPool = m_pool,
  //         .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
  //         .pSetLayouts = layouts.data(),
  //     });
}

std::vector<vk::PipelineShaderStageCreateInfo> ShaderProgram::getStages()
    const {
  return m_stages | std::views::transform([this](auto&& pair) {
           auto&& [type, name] = pair;
           return vk::PipelineShaderStageCreateInfo{
               .stage = type,
               .module = m_program,
               .pName = name.c_str(),
           };
         }) |
         std::ranges::to<std::vector>();
}

std::vector<vk::DescriptorSetLayout> ShaderProgram::getLayouts() const {
  return m_layouts |
         std::views::transform(
             [this](const vk::raii::DescriptorSetLayout& layout) {
               return *layout;
             }) |
         std::ranges::to<std::vector>();
}

void ShaderProgram::updateDescriptorSets() {
  vk::DescriptorType type;
  switch (type) {
    case vk::DescriptorType::eUniformBuffer:
      break;
    case vk::DescriptorType::eSampler:
      break;
    case vk::DescriptorType::eStorageImage:
      break;
    case vk::DescriptorType::eSampledImage:
      break;
    case vk::DescriptorType::eStorageTexelBuffer:
      break;
    case vk::DescriptorType::eUniformTexelBuffer:
      break;
    case vk::DescriptorType::eStorageBuffer:
      break;
    case vk::DescriptorType::eAccelerationStructureKHR:
      break;
    case vk::DescriptorType::eInputAttachment:
      break;
    default:
      throw std::runtime_error(
          std::format("Unsupported descriptor type:{}", vk::to_string(type)));
  };
}

ShaderFactory::ShaderFactory(vk::raii::PhysicalDevice& physicalDevice,
                             vk::raii::Device& device, uint32_t queueIndex)
    : m_physicalDevice(physicalDevice),
      m_device(device),
      m_queueIndex(queueIndex) {}

std::unique_ptr<ShaderBuilder> ShaderFactory::NewBuilder() {
  return std::make_unique<ShaderBuilder>(weak_from_this(), m_device);
}

ShaderBuilder::ShaderBuilder(std::weak_ptr<ShaderFactory> factory,
                             const vk::raii::Device& device)
    : m_factory(factory),
      m_device(device),
      m_spvPath("shaders/vertex_buffer.spv"),
      m_reflectionPath("shaders/vertex_buffer.json") {}

ShaderBuilder& ShaderBuilder::setShader(const std::filesystem::path& path) {
  m_spvPath = path;
  return *this;
}

ShaderBuilder& ShaderBuilder::setReflection(const std::filesystem::path& path) {
  m_reflectionPath = path;
  return *this;
}

size_t ShaderBuilder::build() {
  if (auto factory = m_factory.lock()) {
    return factory->add(ShaderProgram(m_device, m_spvPath, m_reflectionPath));
  }
  return 0;
}

PipelineFactory::PipelineFactory(const vk::raii::Device& device)
    : m_device(device) {}

size_t PipelineFactory::NewPipeline(const ShaderProgram& program,
  const PipelineState& state,
  vk::Format format) {
  std::vector<vk::DescriptorSetLayout> layouts = program.getLayouts();
  std::vector<vk::PipelineShaderStageCreateInfo> stages = program.getStages();
  m_layouts.emplace_back(
      m_device, vk::PipelineLayoutCreateInfo{
                    .setLayoutCount = static_cast<uint32_t>(layouts.size()),
                    .pSetLayouts = layouts.data(),
                    .pushConstantRangeCount = 0,
                });

  constexpr std::array k_dynamicStates = {vk::DynamicState::eViewport,
                                          vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(k_dynamicStates.size()),
      .pDynamicStates = k_dynamicStates.data(),
  };
  constexpr vk::PipelineViewportStateCreateInfo viewportState{
      .sType = vk::StructureType::ePipelineViewportStateCreateInfo,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  vk::StructureChain pipelineCreateInfoChain = {
      vk::GraphicsPipelineCreateInfo{
          .stageCount = static_cast<uint32_t>(stages.size()),
          .pStages = stages.data(),
          .pVertexInputState = &state.vertexInputInfo,
          .pInputAssemblyState = &state.inputAssembly,
          .pViewportState = &viewportState,
          .pRasterizationState = &state.rasterizer,
          .pMultisampleState = &state.multisampling,
          .pDepthStencilState = &state.depthStencil,
          .pColorBlendState = &state.colorBlending,
          .pDynamicState = &dynamicState,
          .layout = m_layouts.back(),
          .renderPass = nullptr,
      },
      vk::PipelineRenderingCreateInfo{
          .colorAttachmentCount = 1,
          .pColorAttachmentFormats = &format,
          .depthAttachmentFormat = vk::Format::eUndefined,
      },
  };
  return add(
      std::tie(program, state),
      vk::raii::Pipeline(m_device, nullptr, pipelineCreateInfoChain.get()));
}

vk::Pipeline PipelineFactory::Pipeline(const ShaderProgram& program,
                                       const PipelineState& state) {
  const size_t id =
      std::hash<std::tuple<const ShaderProgram&, const PipelineState&>>{}(
          std::tie(program, state));
  return *operator[](id);
}

vk::PipelineLayout PipelineFactory::Layout(size_t id) {
  return *m_layouts[m_table[id]];
}

LayerSystem::LayerSystem(std::shared_ptr<RenderSystem> renderSystem)
    : m_renderer(renderSystem->GetRenderer()) {}

void LayerSystem::Update(FrameData& frameData) {
  if (m_layers.empty()) {
    return;
  }
  Layer& layer = *m_layers.top();
  layer.Update();
  if (auto root = layer.GetRoot().lock()) {
    std::vector<Vertex> mesh = root->getMesh();
    frameData.bufferBuilder->SetVertices(mesh);
    frameData.vertexBuilder->SetVertices(mesh);
  }
}

void LayerSystem::Push(std::shared_ptr<Layer> layer) {
  m_layers.push(std::move(layer));
}

std::shared_ptr<Layer> LayerSystem::Pop() {
  auto layer = std::move(m_layers.top());
  m_layers.pop();
  return layer;
}

/* HASH DECLARATION */

template <>
struct std::hash<Vertex> {
  size_t operator()(Vertex const& vertex) const noexcept {
    return ((std::hash<glm::vec2>()(vertex.pos) ^
             (std::hash<glm::vec3>()(vertex.color) << 1)) >>
            1);
  }
};

template <>
struct std::hash<ShaderProgram> {
  size_t operator()(ShaderProgram const& program) const noexcept {
    return program.m_hash;
  }
};

template <>
struct std::hash<vk::VertexInputAttributeDescription> {
  size_t operator()(
      vk::VertexInputAttributeDescription const& description) const noexcept {
    return hash_tuple(std::tie(description.format, description.binding,
                               description.location, description.offset));
  }
};

template <>
struct std::hash<vk::VertexInputBindingDescription> {
  size_t operator()(
      vk::VertexInputBindingDescription const& description) const noexcept {
    return hash_tuple(std::tie(description.binding, description.inputRate,
                               description.stride));
  }
};

template <>
struct std::hash<vk::PipelineVertexInputStateCreateInfo> {
  size_t operator()(
      vk::PipelineVertexInputStateCreateInfo const& info) const noexcept {
    auto attribute = std::span(info.pVertexAttributeDescriptions,
                               info.vertexAttributeDescriptionCount);
    auto binding = std::span(info.pVertexBindingDescriptions,
                             info.vertexBindingDescriptionCount);
    return hash_tuple(std::tie(attribute, binding));
  }
};

template <>
struct std::hash<vk::PipelineInputAssemblyStateCreateInfo> {
  size_t operator()(
      vk::PipelineInputAssemblyStateCreateInfo const& info) const noexcept {
    return hash_tuple(std::tie(info.primitiveRestartEnable, info.topology));
  }
};

template <>
struct std::hash<vk::PipelineRasterizationStateCreateInfo> {
  size_t operator()(
      vk::PipelineRasterizationStateCreateInfo const& info) const noexcept {
    auto polygon = static_cast<int>(info.polygonMode);
    auto cull = static_cast<uint32_t>(info.cullMode);
    return hash_tuple(std::tie(info.depthClampEnable,
                               info.rasterizerDiscardEnable, polygon, cull,
                               info.frontFace, info.depthBiasEnable,
                               info.depthBiasSlopeFactor, info.lineWidth));
  }
};

template <>
struct std::hash<vk::PipelineDepthStencilStateCreateInfo> {
  size_t operator()(
      vk::PipelineDepthStencilStateCreateInfo const& info) const noexcept {
    return hash_tuple(std::tie(info.depthTestEnable, info.depthWriteEnable,
                               info.depthCompareOp, info.depthBoundsTestEnable,
                               info.stencilTestEnable));
  }
};

template <>
struct std::hash<vk::PipelineMultisampleStateCreateInfo> {
  size_t operator()(
      vk::PipelineMultisampleStateCreateInfo const& info) const noexcept {
    return hash_tuple(
        std::tie(info.rasterizationSamples, info.sampleShadingEnable,
                 info.alphaToCoverageEnable, info.alphaToOneEnable));
  }
};

template <>
struct std::hash<vk::PipelineColorBlendAttachmentState> {
  size_t operator()(
      vk::PipelineColorBlendAttachmentState const& attachment) const noexcept {
    uint32_t mask = static_cast<uint32_t>(attachment.colorWriteMask);
    return hash_tuple(std::tie(mask, attachment.blendEnable));
  }
};

template <>
struct std::hash<vk::PipelineColorBlendStateCreateInfo> {
  size_t operator()(
      vk::PipelineColorBlendStateCreateInfo const& info) const noexcept {
    auto attachment = std::span(info.pAttachments, info.attachmentCount);
    return hash_tuple(std::tie(info.logicOpEnable, info.logicOp, attachment));
  }
};

template <>
struct std::hash<PipelineState> {
  size_t operator()(PipelineState const& state) const noexcept {
    return hash_tuple(std::tie(state.vertexInputInfo, state.inputAssembly,
                               state.rasterizer, state.multisampling,
                               state.depthStencil, state.colorBlending));
  }
};
