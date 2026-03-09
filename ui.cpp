#include "base.cpp"

Slate::Slate(std::shared_ptr<ISlate> parent) : m_parent(parent) {}

void Slate::Add(std::shared_ptr<ISlate> slate) {
  m_children.push_back(std::move(slate));
}

void Slate::Remove(std::shared_ptr<ISlate> slate) {
  std::erase_if(m_children, [slate](std::shared_ptr<ISlate> child) {
    return child.get() == slate.get();
  });
}

std::shared_ptr<ISlate> Slate::GetRoot() {
  if (auto parent = m_parent.lock()) {
    return parent->GetRoot();
  }
  return shared_from_this();
}

std::vector<Vertex> Slate::getMesh() {
  if (m_children.empty()) {
    return {
        {m_position, {}},
        {{m_position.x + m_extent.x, m_position.y}, {}},
        {{m_position.x, m_position.y + m_extent.y}, {}},
        {{m_position.x + m_extent.x, m_position.y + m_extent.y}, {}},
    };
  }

  std::vector<Vertex> vertices;
  std::ranges::for_each(
      m_children | std::views::transform(
                       [](std::weak_ptr<ISlate> child) -> std::vector<Vertex> {
                         if (auto slate = child.lock()) {
                           return slate->getMesh();
                         } else {
                           return {};
                         }
                       }),
      [&vertices](const std::vector<Vertex> v) {
        std::move(v.begin(), v.end(), std::back_inserter(vertices));
      });
  return vertices;
}

void FlexLayout::Update() {
  m_extent = m_position;
  std::ranges::for_each(m_children, [this](std::weak_ptr<ISlate> p) {
    if (auto slate = p.lock()) {
      slate->SetPosition(m_extent);
      slate->Update();
      auto extent = slate->GetExtent();
      m_extent.x += extent.x;
      m_extent.y = std::max(m_extent.y, extent.y);
    }
  });
}

void Layer::Update() {
  if (!m_root) {
    return;
  }
  m_root->Update();
}

void Layer::SetRoot(std::shared_ptr<ISlate> slate) { m_root = slate; }

std::weak_ptr<ISlate> Layer::GetRoot() { return m_root; }

BaseMenu::BaseMenu() {
  auto layout = std::make_shared<FlexLayout>();
  SetRoot(layout);
  auto button = std::make_shared<Slate>();
  layout->Add(button);
}
