#include "base.cpp"

Application::Application() : m_window(std::make_unique<Window>()) {}

void Application::run() { m_window->Update(); }

Window::Window() {
  glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  m_window = glfwCreateWindow(k_width, k_height, Application::k_name.data(),
                              nullptr, nullptr);
  m_renderSystem = std::make_shared<RenderSystem>(*this);
  m_layerSystem = std::make_unique<LayerSystem>(m_renderSystem);
  m_layerSystem->Push(std::make_shared<BaseMenu>());
}

Window::~Window() { glfwDestroyWindow(m_window); }

void Window::Update() {
  while (!glfwWindowShouldClose(m_window)) {
    glfwPollEvents();
    FrameData frameData = m_renderSystem->PreDraw();
    m_layerSystem->Update(frameData);
    m_renderSystem->Draw(frameData);
    m_renderSystem->PostDraw(frameData);
  }
}

std::vector<const char*> Window::getExtensions() {
  unsigned int glfwExtensionCount = 0;
  auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  return {glfwExtensions, glfwExtensions + glfwExtensionCount};
}

VkSurfaceKHR Window::createSurface(const VkInstance& instance) {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != 0) {
    throw std::runtime_error("Failed to create window surface!");
  }
  return surface;
}

VkExtent2D Window::getExtent() {
  VkExtent2D extent;
  glfwGetFramebufferSize(m_window, reinterpret_cast<int*>(&extent.width),
                         reinterpret_cast<int*>(&extent.height));
  return extent;
}
