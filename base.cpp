#pragma once

#include <algorithm>
#include <filesystem>
#include <memory>
#include <print>
#include <ranges>
#include <set>
#include <span>
#include <stack>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>

#include <glaze/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_raii.hpp>

class Application;
class Window;
class RenderSystem;
class Renderer;
class ShaderProgram;
class Vertex;
struct PipelineState;
struct FrameData;
class ShaderFactory;
class ShaderBuilder;
class PipelineFactory;
class BufferFactory;
class VertexBuilder;
class StagingBufferBuilder;
class ISlate;
class Slate;
class FlexLayout;
class Layer;
class LayerSystem;

class Application {
 public:
  static constexpr std::string_view k_name = "FAHHHHH";

 public:
  Application();

  void run();

 private:
  std::unique_ptr<Window> m_window;
};

class Window {
  static constexpr int k_width = 640;
  static constexpr int k_height = 480;

 public:
  Window();

  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  void Update();
  std::vector<const char*> getExtensions();
  VkSurfaceKHR createSurface(const VkInstance& instance);
  VkExtent2D getExtent();

 private:
  GLFWwindow* m_window;
  std::shared_ptr<RenderSystem> m_renderSystem;
  std::unique_ptr<LayerSystem> m_layerSystem;
};

class RenderSystem {
 public:
  RenderSystem(Window& window);
  FrameData PreDraw();
  void Draw(FrameData& frameData);
  void PostDraw(FrameData& frameData);
  std::shared_ptr<Renderer> GetRenderer();

 private:
  std::shared_ptr<Renderer> m_renderer;
};

class Renderer {
 public:
  Renderer(VkSurfaceKHR surface, vk::raii::Context context,
           vk::raii::Instance instance,
           vk::raii::DebugUtilsMessengerEXT debugger,
           vk::raii::PhysicalDevice physicalDevice,
           vk::raii::Device logicalDevice, uint32_t graphicsQueueIdx,
           uint32_t presentQueueIdx, vk::SurfaceCapabilitiesKHR capabilities,
           vk::SurfaceFormatKHR surfaceFormat, vk::Extent2D swapchainExtent,
           vk::PresentModeKHR presentMode);
  FrameData PreSubmit();
  void Submit(FrameData& frameData);
  void PostSubmit(FrameData& frameData);

 private:
  void transitionImageLayout(const vk::raii::CommandBuffer& commandBuffer,
                             const vk::Image& image, vk::ImageLayout oldLayout,
                             vk::ImageLayout newLayout);

 private:
  vk::raii::Context m_context;
  vk::raii::DebugUtilsMessengerEXT m_debugger;
  vk::raii::Instance m_instance;
  vk::raii::SurfaceKHR m_surface;
  vk::raii::PhysicalDevice m_physicalDevice;
  vk::raii::Device m_device;
  vk::raii::Queue m_graphicsQueue;
  vk::raii::Queue m_presentQueue;
  vk::Extent2D m_swapChainExtent;
  vk::raii::SwapchainKHR m_swapChain;
  std::vector<vk::Image> m_swapChainImages;
  std::vector<vk::raii::ImageView> m_swapChainImageViews;
  std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
  vk::raii::Semaphore m_presentCompletedSemaphore;
  vk::raii::Fence m_drawFence;

  std::shared_ptr<ShaderFactory> m_shadersFactory;
  std::shared_ptr<BufferFactory> m_buffersFactory;
  std::shared_ptr<BufferFactory> m_stagingBuffersFactory;
  std::unique_ptr<PipelineFactory> m_pipelinesFactory;
  vk::raii::CommandBuffer m_commandBuffer;
};

class ShaderProgram {
 public:
  friend struct std::hash<ShaderProgram>;

  ShaderProgram(const vk::raii::Device& device,
                const std::filesystem::path& spv,
                const std::filesystem::path& reflection);
  std::vector<vk::PipelineShaderStageCreateInfo> getStages() const;
  std::vector<vk::DescriptorSetLayout> getLayouts() const;
  void updateDescriptorSets();

 private:
  size_t m_hash = 0;
  vk::raii::DescriptorPool m_pool;
  vk::raii::ShaderModule m_program;

  std::unordered_map<vk::ShaderStageFlagBits, std::string> m_stages;
  std::unordered_map<vk::DescriptorType, uint32_t> m_poolTypes;
  std::vector<vk::raii::DescriptorSetLayout> m_layouts;
  std::vector<vk::raii::DescriptorSet> m_descriptorSets;
};

static inline size_t fnv1a(std::span<char> data) {
  constexpr size_t hash = 14695981039346656037ULL;  // FNV offset basis
  constexpr size_t prime = 1099511628211ULL;
  return std::ranges::fold_left(data, hash, [](size_t lhs, size_t rhs) {
    return (lhs ^ static_cast<unsigned char>(rhs)) * prime;
  });
}

template <class T>
static inline void hash_combine(std::size_t& seed, const T& v) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Ts>
static size_t hash_tuple(const std::tuple<Ts...>& t) {
  size_t seed = 0;
  std::apply(
      [&seed](const auto&... elems) {
        (...,
         hash_combine(seed, std::hash<std::decay_t<decltype(elems)>>{}(elems)));
      },
      t);
  return seed;
}

template <typename... Ts>
struct std::hash<std::tuple<Ts...>> {
  std::size_t operator()(const std::tuple<Ts...>& t) const noexcept {
    return hash_tuple(t);
  }
};

template <typename T, std::size_t Extent>
struct std::hash<std::span<T, Extent>> {
  std::size_t operator()(std::span<T, Extent> s) const noexcept {
    return std::ranges::fold_left(s, s.size(),
                                  [](std::size_t seed, const T& elem) {
                                    hash_combine(seed, elem);
                                    return seed;
                                  });
  }
};

struct Vertex {
  alignas(8) glm::vec2 pos;
  alignas(16) glm::vec3 color;

  static inline constexpr auto getBindingDescription() {
    return std::array{
        vk::VertexInputBindingDescription(0, sizeof(Vertex),
                                          vk::VertexInputRate::eVertex),
    };
  }

  static inline constexpr auto getAttributeDescriptions() {
    return std::array{
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat,
                                            offsetof(Vertex, pos)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                            offsetof(Vertex, color)),
    };
  }

  bool operator==(const Vertex& other) const;
};

struct PipelineState {
  static constexpr auto bindingDescriptions = Vertex::getBindingDescription();
  static constexpr auto attributeDescriptions =
      Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = bindingDescriptions.size(),
      .pVertexBindingDescriptions = bindingDescriptions.data(),
      .vertexAttributeDescriptionCount = attributeDescriptions.size(),
      .pVertexAttributeDescriptions = attributeDescriptions.data(),
  };

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList,
      .primitiveRestartEnable = vk::False,
  };

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eClockwise,
      .depthBiasEnable = vk::False,
      .depthBiasSlopeFactor = 1.0f,
      .lineWidth = 1.0f,
  };

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False,
      .alphaToCoverageEnable = vk::False,
      .alphaToOneEnable = vk::False,
  };

  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::False,
      .depthWriteEnable = vk::False,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False,
  };

  static constexpr vk::PipelineColorBlendAttachmentState colorBlendAttachment{
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

  bool operator==(const PipelineState& other);
};

struct FrameData {
  vk::raii::CommandBuffer& commandBuffer;
  uint32_t imageIndex;
  std::unique_ptr<ShaderBuilder> shaderBuilder;
  std::unique_ptr<StagingBufferBuilder> bufferBuilder;
  std::unique_ptr<VertexBuilder> vertexBuilder;
};

template <class T>
class Allocator {
 protected:
  size_t add(T&& data);
  template <class... Args>
  size_t add(std::tuple<const Args&...> args, T&& data);
  template <class... Args>
  size_t add(std::tuple<Args...> args, T&& data);
  T& operator[](size_t id);

 protected:
  std::vector<T> m_data;
  std::unordered_map<size_t, size_t> m_table;
};

class ShaderFactory : public Allocator<ShaderProgram>,
                      public std::enable_shared_from_this<ShaderFactory> {
  friend ShaderBuilder;

 public:
  ShaderFactory(vk::raii::PhysicalDevice& physicalDevice,
                vk::raii::Device& device, uint32_t queueIndex);
  std::unique_ptr<ShaderBuilder> NewBuilder();

 private:
  const vk::raii::PhysicalDevice& m_physicalDevice;
  const vk::raii::Device& m_device;
  const uint32_t m_queueIndex;
};

class ShaderBuilder {
 public:
  ShaderBuilder(std::weak_ptr<ShaderFactory> factory,
                const vk::raii::Device& device);
  ShaderBuilder& setShader(const std::filesystem::path& path);
  ShaderBuilder& setReflection(const std::filesystem::path& path);
  size_t build();

 private:
  std::weak_ptr<ShaderFactory> m_factory;
  const vk::raii::Device& m_device;
  std::filesystem::path m_spvPath;
  std::filesystem::path m_reflectionPath;
};

class PipelineFactory : public Allocator<vk::raii::Pipeline> {
 public:
  PipelineFactory(const vk::raii::Device& device);
  size_t NewPipeline(const ShaderProgram& program, const PipelineState& state,
                     vk::Format format);
  vk::Pipeline Pipeline(const ShaderProgram& program,
                        const PipelineState& state);
  vk::PipelineLayout Layout(size_t id);

 private:
  const vk::raii::Device& m_device;
  std::vector<vk::raii::PipelineLayout> m_layouts;
};

class BufferFactory : public Allocator<vk::raii::Buffer>,
                      public std::enable_shared_from_this<BufferFactory> {
 public:
  BufferFactory(vk::raii::PhysicalDevice& physicalDevice,
                vk::raii::Device& device, uint32_t queueIndex);

  template <class T>
  size_t NewStagingBuffer(std::span<T> data);
  template <class T>
  size_t NewVertexBuffer(std::span<T> data);
  template <class T>
  size_t NewIndexBuffer(std::span<T> data);
  vk::raii::CommandBuffers NewCommandBuffers(uint32_t count);
  vk::raii::Buffer& Buffer(size_t id);
  std::unique_ptr<StagingBufferBuilder> NewStagingBuilder();
  std::unique_ptr<VertexBuilder> NewBuilder();

 private:
  vk::raii::DeviceMemory createDeviceMemory(const vk::raii::Buffer& buffer,
                                            vk::MemoryPropertyFlags property);

 private:
  const vk::raii::PhysicalDevice& m_physicalDevice;
  const vk::raii::Device& m_device;

  vk::raii::CommandPool m_commandPool;
  std::vector<vk::raii::DeviceMemory> m_memories;
};

class VertexBuilder {
 public:
  VertexBuilder(std::weak_ptr<BufferFactory> factory);
  VertexBuilder& SetVertices(const std::vector<Vertex>& vertices);
  size_t Build();
  uint64_t Size();
  size_t ID();

 private:
  size_t m_id = 0;
  std::weak_ptr<BufferFactory> m_factory;
  std::vector<Vertex> m_vertices;
  uint64_t m_size = 0;
};

class StagingBufferBuilder {
 public:
  StagingBufferBuilder(std::weak_ptr<BufferFactory> factory);
  StagingBufferBuilder& SetVertices(const std::vector<Vertex>& vertices);
  size_t Build();
  size_t ID();

 private:
  size_t m_id = 0;
  std::weak_ptr<BufferFactory> m_factory;
  std::vector<Vertex> m_vertices;
};

class ISlate {
 public:
  virtual void Update() = 0;
  virtual glm::vec2 GetPosition() = 0;
  virtual void SetPosition(glm::vec2 position) = 0;
  virtual glm::vec2 GetExtent() = 0;
  virtual void SetExtent(glm::vec2 extent) = 0;
  virtual void Add(std::shared_ptr<ISlate> slate) = 0;
  virtual void Remove(std::shared_ptr<ISlate> slate) = 0;
  virtual std::shared_ptr<ISlate> GetRoot() = 0;
  virtual std::vector<Vertex> getMesh() = 0;
};

class Slate : public ISlate, std::enable_shared_from_this<Slate> {
 public:
  Slate(std::shared_ptr<ISlate> parent = nullptr);

  void Update() override {};
  glm::vec2 GetPosition() override { return m_position; }
  void SetPosition(glm::vec2 position) override { m_position = position; }
  glm::vec2 GetExtent() override { return m_extent; }
  void SetExtent(glm::vec2 extent) override { m_extent = extent; }
  void Add(std::shared_ptr<ISlate> slate) override;
  void Remove(std::shared_ptr<ISlate> slate) override;
  std::shared_ptr<ISlate> GetRoot() override;
  std::vector<Vertex> getMesh() override;

 protected:
  std::weak_ptr<ISlate> m_parent;
  std::vector<std::shared_ptr<ISlate>> m_children;

  glm::vec2 m_position;
  glm::vec2 m_extent;
  std::string m_name;
};

class FlexLayout : public Slate {
 public:
  FlexLayout() = default;
  void Update() override;
};

class Layer {
 public:
  virtual void Update();
  void SetRoot(std::shared_ptr<ISlate> slate);
  std::weak_ptr<ISlate> GetRoot();

 private:
  std::shared_ptr<ISlate> m_root;
};

class LayerSystem {
 public:
  LayerSystem(std::shared_ptr<RenderSystem> renderSystem);
  void Update(FrameData& data);
  void Push(std::shared_ptr<Layer> layer);
  std::shared_ptr<Layer> Pop();

 private:
  std::stack<std::shared_ptr<Layer>> m_layers;
  std::shared_ptr<Renderer> m_renderer;
};

class BaseMenu : public Layer {
 public:
  BaseMenu();
};
