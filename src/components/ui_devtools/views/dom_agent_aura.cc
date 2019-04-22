// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/dom_agent_aura.h"

#include "base/stl_util.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace ui_devtools {

namespace {
using ui_devtools::protocol::Array;
using ui_devtools::protocol::DOM::Node;
}  // namespace

DOMAgentAura* DOMAgentAura::dom_agent_aura_ = nullptr;

DOMAgentAura::DOMAgentAura() {
  DCHECK(!dom_agent_aura_);
  dom_agent_aura_ = this;
  RegisterEnv(aura::Env::GetInstance());
}
DOMAgentAura::~DOMAgentAura() {
  for (aura::Window* window : roots_)
    window->RemoveObserver(this);
  for (auto* env : envs_)
    env->RemoveObserver(this);
  dom_agent_aura_ = nullptr;
}

void DOMAgentAura::RegisterEnv(aura::Env* env) {
  envs_.push_back(env);
  env->AddObserver(this);
}

void DOMAgentAura::RegisterRootWindow(aura::Window* root) {
  roots_.push_back(root);
  root->AddObserver(this);
}

void DOMAgentAura::OnHostInitialized(aura::WindowTreeHost* host) {
  RegisterRootWindow(host->window());
}

void DOMAgentAura::OnWindowDestroying(aura::Window* window) {
  base::Erase(roots_, window);
}

std::vector<UIElement*> DOMAgentAura::CreateChildrenForRoot() {
  std::vector<UIElement*> children;
  for (aura::Window* window : roots_) {
    UIElement* window_element = new WindowElement(window, this, element_root());
    children.push_back(window_element);
  }
  return children;
}

std::unique_ptr<Node> DOMAgentAura::BuildTreeForWindow(
    UIElement* window_element_root) {
  DCHECK(window_element_root->type() == UIElementType::WINDOW);
  aura::Window* window =
      UIElement::GetBackingElement<aura::Window, WindowElement>(
          window_element_root);
  std::unique_ptr<Array<Node>> children = Array<Node>::create();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    UIElement* widget_element =
        new WidgetElement(widget, this, window_element_root);

    children->addItem(BuildTreeForRootWidget(widget_element));
    window_element_root->AddChild(widget_element);
  }
  for (aura::Window* child : window->children()) {
    UIElement* window_element =
        new WindowElement(child, this, window_element_root);

    children->addItem(BuildTreeForWindow(window_element));
    window_element_root->AddChild(window_element);
  }
  std::unique_ptr<Node> node =
      BuildNode("Window", window_element_root->GetAttributes(),
                std::move(children), window_element_root->node_id());
  return node;
}

// static
std::unique_ptr<DOMAgentViews> DOMAgentViews::Create() {
  return std::make_unique<DOMAgentAura>();
}

}  // namespace ui_devtools
