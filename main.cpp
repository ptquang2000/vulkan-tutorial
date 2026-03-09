#include "app.cpp"
#include "base.cpp"
#include "renderer.cpp"
#include "ui.cpp"

int main() {
  try {
    auto* app = new Application();
    app->run();
    delete app;
  } catch (const std::exception& e) {
    std::println("Error: {}", e.what());
  }
}
