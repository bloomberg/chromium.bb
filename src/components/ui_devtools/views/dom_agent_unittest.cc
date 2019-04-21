// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/ui_devtools/views/dom_agent_views.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/overlay_agent_views.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#endif  // defined(USE_AURA)

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;

class TestView : public views::View {
 public:
  TestView(const char* name) : name_(name) {}

  const char* GetClassName() const override { return name_; }

 private:
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

std::string GetAttributeValue(const std::string& attribute, DOM::Node* node) {
  EXPECT_TRUE(node->hasAttributes());
  Array<std::string>* attributes = node->getAttributes(nullptr);
  for (size_t i = 0; i < attributes->length() - 1; i++) {
    if (attributes->get(i) == attribute)
      return attributes->get(i + 1);
  }
  return std::string();
}

DOM::Node* FindNodeWithID(int id, DOM::Node* root) {
  if (id == root->getNodeId()) {
    return root;
  }
  Array<DOM::Node>* children = root->getChildren(nullptr);
  for (size_t i = 0; i < children->length(); i++) {
    if (DOM::Node* node = FindNodeWithID(id, children->get(i)))
      return node;
  }
  return nullptr;
}

}  // namespace

class DOMAgentTest : public views::ViewsTestBase {
 public:
  DOMAgentTest() {}
  ~DOMAgentTest() override {}

  views::internal::NativeWidgetPrivate* CreateTestNativeWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
#if defined(USE_AURA)
    params.parent = GetContext();
#endif
    widget->Init(params);
    return widget->native_widget_private();
  }

  std::unique_ptr<views::Widget> CreateTestWidget(const gfx::Rect& bounds) {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.delegate = nullptr;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = bounds;
#if defined(USE_AURA)
    params.parent = GetContext();
#endif
    widget->Init(params);
    widget->Show();
    return widget;
  }

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> CreateChildWindow(
      aura::Window* parent,
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL) {
    std::unique_ptr<aura::Window> window =
        std::make_unique<aura::Window>(nullptr, type);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->SetBounds(gfx::Rect());
    parent->AddChild(window.get());
    window->Show();
    return window;
  }
#endif

  void SetUp() override {
    fake_frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ =
        std::make_unique<UberDispatcher>(fake_frontend_channel_.get());
    dom_agent_ = DOMAgentViews::Create();
    dom_agent_->Init(uber_dispatcher_.get());
    css_agent_ = std::make_unique<CSSAgent>(dom_agent_.get());
    css_agent_->Init(uber_dispatcher_.get());
    css_agent_->enable();
    overlay_agent_ = OverlayAgentViews::Create(dom_agent_.get());
    overlay_agent_->Init(uber_dispatcher_.get());
    overlay_agent_->enable();

    // We need to create |dom_agent| first to observe creation of
    // WindowTreeHosts in ViewTestBase::SetUp().
    views::ViewsTestBase::SetUp();

#if defined(USE_AURA)
    top_window = CreateChildWindow(GetContext());
#endif
  }

  void TearDown() override {
#if defined(USE_AURA)
    top_window.reset();
#endif
    css_agent_.reset();
    overlay_agent_.reset();
    dom_agent_.reset();

    uber_dispatcher_.reset();
    fake_frontend_channel_.reset();
    views::ViewsTestBase::TearDown();
  }

  template <typename T>
  bool WasChildNodeInserted(T* parent, int previous_sibling_id = 0) {
    const int parent_id = GetIDForBackendElement(parent);
    return frontend_channel()->CountProtocolNotificationMessageStartsWith(
               base::StringPrintf("{\"method\":\"DOM.childNodeInserted\","
                                  "\"params\":{\"parentNodeId\":%d,"
                                  "\"previousNodeId\":%d",
                                  parent_id, previous_sibling_id)) == 1;
  }
  template <typename T, typename U>
  bool WasChildNodeInserted(T* parent, U* previous_sibling) {
    const int previous_sibling_id =
        previous_sibling ? GetIDForBackendElement(previous_sibling) : 0;
    return WasChildNodeInserted(parent, previous_sibling_id);
  }

  template <typename T>
  bool WasChildNodeRemoved(T* parent, int node_id) {
    const int parent_id = GetIDForBackendElement(parent);
    return frontend_channel()->CountProtocolNotificationMessage(
               base::StringPrintf(
                   "{\"method\":\"DOM.childNodeRemoved\",\"params\":{"
                   "\"parentNodeId\":%d,\"nodeId\":%d}}",
                   parent_id, node_id)) == 1;
  }

  template <typename T>
  int GetIDForBackendElement(T* element) {
    return dom_agent()->element_root()->FindUIElementIdForBackendElement<T>(
        element);
  }

  template <typename T>
  DOM::Node* FindInRoot(T* element, DOM::Node* root) {
    return FindNodeWithID(GetIDForBackendElement(element), root);
  }

  // The following three methods test that a tree of DOM::Node exactly
  // corresponds 1:1 with a views/Aura hierarchy

#if defined(USE_AURA)
  bool ElementTreeMatchesDOMTree(aura::Window* window, DOM::Node* root) {
    if (GetIDForBackendElement(window) != root->getNodeId() ||
        "Window" != root->getNodeName() ||
        window->GetName() != GetAttributeValue("name", root)) {
      return false;
    }

    Array<DOM::Node>* children = root->getChildren(nullptr);
    size_t child_index = 0;
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    if (widget &&
        !ElementTreeMatchesDOMTree(widget, children->get(child_index++))) {
      return false;
    }
    for (aura::Window* child_window : window->children()) {
      if (!ElementTreeMatchesDOMTree(child_window,
                                     children->get(child_index++)))
        return false;
    }
    // Make sure there are no stray children.
    return child_index == children->length();
  }
#endif

  bool ElementTreeMatchesDOMTree(views::Widget* widget, DOM::Node* root) {
    if (GetIDForBackendElement(widget) != root->getNodeId() ||
        "Widget" != root->getNodeName() ||
        widget->GetName() != GetAttributeValue("name", root)) {
      return false;
    }

    Array<DOM::Node>* children = root->getChildren(nullptr);
    views::View* root_view = widget->GetRootView();
    if (!root_view)
      return children->length() == 0;
    else
      return ElementTreeMatchesDOMTree(root_view, children->get(0));
  }

  bool ElementTreeMatchesDOMTree(views::View* view, DOM::Node* root) {
    if (GetIDForBackendElement(view) != root->getNodeId() ||
        "View" != root->getNodeName() ||
        view->GetClassName() != GetAttributeValue("name", root)) {
      return false;
    }

    Array<DOM::Node>* children = root->getChildren(nullptr);
    std::vector<views::View*> child_views = view->GetChildrenInZOrder();
    const size_t child_count = child_views.size();
    if (child_count != children->length())
      return false;

    for (size_t i = 0; i < child_count; i++) {
      if (!ElementTreeMatchesDOMTree(child_views[i], children->get(i)))
        return false;
    }
    return true;
  }

  FakeFrontendChannel* frontend_channel() {
    return fake_frontend_channel_.get();
  }
  DOMAgentViews* dom_agent() { return dom_agent_.get(); }

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> top_window;
#endif
 private:
  std::unique_ptr<UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeFrontendChannel> fake_frontend_channel_;
  std::unique_ptr<DOMAgentViews> dom_agent_;
  std::unique_ptr<CSSAgent> css_agent_;
  std::unique_ptr<OverlayAgentViews> overlay_agent_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgentTest);
};

// Tests to ensure that the DOMAgent's hierarchy matches the real hierarchy.
#if defined(USE_AURA)

TEST_F(DOMAgentTest, GetDocumentWithWindowWidgetView) {
  // parent_window
  //   widget
  //     (root/content views)
  //        child_view
  //   child_window
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));
  aura::Window* parent_window = widget->GetNativeWindow();
  parent_window->SetName("parent_window");
  std::unique_ptr<aura::Window> child_window = CreateChildWindow(parent_window);
  child_window->SetName("child_window");
  widget->Show();
  views::View* child_view = new TestView("child_view");
  widget->GetRootView()->AddChildView(child_view);

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  DCHECK(parent_node);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(parent_window, parent_node));
}

TEST_F(DOMAgentTest, GetDocumentNativeWidgetOwnsWidget) {
  views::internal::NativeWidgetPrivate* native_widget_private =
      CreateTestNativeWidget();
  views::Widget* widget = native_widget_private->GetWidget();
  aura::Window* parent_window = widget->GetNativeWindow();

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  DCHECK(parent_node);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(parent_window, parent_node));
  // Destroy NativeWidget followed by |widget|
  widget->CloseNow();
}

#endif  // defined(USE_AURA)

TEST_F(DOMAgentTest, GetDocumentMultipleWidgets) {
  // widget_a
  //   (root/contents views)
  //     child_a1
  //     child_a2
  // widget_b
  //   (root/contents views)
  //      child_b1
  //        child_b11
  //          child_b111
  //            child_b1111
  //          child_b112
  //          child_b113
  //        child_b12
  //          child_b121
  //          child_b122
  std::unique_ptr<views::Widget> widget_a(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));
  std::unique_ptr<views::Widget> widget_b(
      CreateTestWidget(gfx::Rect(100, 100, 80, 80)));
  widget_a->GetRootView()->AddChildView(new TestView("child_a1"));
  widget_a->GetRootView()->AddChildView(new TestView("child_a2"));

  auto child_b1 = std::make_unique<TestView>("child_b1");
  child_b1->set_owned_by_client();
  widget_b->GetRootView()->AddChildView(child_b1.get());

  auto child_b11 = std::make_unique<TestView>("child_b11");
  child_b11->set_owned_by_client();
  child_b1->AddChildView(child_b11.get());

  auto child_b111 = std::make_unique<TestView>("child_b111");
  child_b111->set_owned_by_client();
  child_b11->AddChildView(child_b111.get());
  child_b111->AddChildView(new TestView("child_b1111"));

  child_b11->AddChildView(new TestView("child_b112"));
  child_b11->AddChildView(new TestView("child_b113"));

  auto child_b12 = std::make_unique<TestView>("child_b12");
  child_b12->set_owned_by_client();
  child_b1->AddChildView(child_b12.get());
  child_b12->AddChildView(new TestView("child_b121"));
  child_b12->AddChildView(new TestView("child_b122"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* widget_node_a = FindInRoot(widget_a.get(), root.get());
  DCHECK(widget_node_a);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(widget_a.get(), widget_node_a));

  DOM::Node* widget_node_b = FindInRoot(widget_b.get(), root.get());
  DCHECK(widget_node_b);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(widget_b.get(), widget_node_b));
}

// Tests to ensure correct messages are sent when elements are added,
// removed, and moved around.

#if defined(USE_AURA)
TEST_F(DOMAgentTest, WindowAddedChildNodeInserted) {
  // Initialize DOMAgent
  std::unique_ptr<aura::Window> first_child =
      CreateChildWindow(top_window.get());

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::unique_ptr<aura::Window> second_child(
      CreateChildWindow(top_window.get()));
  EXPECT_TRUE(WasChildNodeInserted(top_window.get(), first_child.get()));
}

TEST_F(DOMAgentTest, WindowDestroyedChildNodeRemoved) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(child_1.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int child_id = GetIDForBackendElement(child_2.get());

  child_2.reset();
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), child_id));
}

TEST_F(DOMAgentTest, WindowReorganizedChildNodeRearranged) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(child_1.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());
  child_2->AddChild(child_11.get());
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), moving_child_id));
  EXPECT_TRUE(WasChildNodeInserted(child_2.get(), child_21.get()));
}

TEST_F(DOMAgentTest, WindowReorganizedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());
  std::unique_ptr<aura::Window> child_22 = CreateChildWindow(child_2.get());
  // Initialized at the end since it will be a child of |child_2| at
  // tear down.
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(child_1.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());

  child_1->RemoveChild(child_11.get());
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), moving_child_id));

  child_2->AddChild(child_11.get());
  EXPECT_TRUE(WasChildNodeInserted(child_2.get(), child_22.get()));
}

TEST_F(DOMAgentTest, WindowStackingChangedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_12 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_13 = CreateChildWindow(top_window.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());
  top_window->StackChildAbove(child_11.get(), child_12.get());
  EXPECT_TRUE(WasChildNodeRemoved(top_window.get(), moving_child_id));
  EXPECT_TRUE(WasChildNodeInserted(top_window.get(), child_12.get()));
}
#endif  // defined(USE_AURA)

TEST_F(DOMAgentTest, ViewInserted) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));
  widget->Show();

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  views::View* root_view = widget->GetRootView();
  root_view->AddChildView(new views::View);
  EXPECT_TRUE(WasChildNodeInserted(root_view, root_view->child_at(0)));
}

TEST_F(DOMAgentTest, ViewRemoved) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));
  widget->Show();
  views::View* root_view = widget->GetRootView();

  // Need to store |child_view| in unique_ptr because it is removed from the
  // widget and needs to be destroyed independently
  std::unique_ptr<views::View> child_view = std::make_unique<views::View>();
  root_view->AddChildView(child_view.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int removed_node_id = GetIDForBackendElement(child_view.get());
  root_view->RemoveChildView(child_view.get());
  EXPECT_TRUE(WasChildNodeRemoved(root_view, removed_node_id));
}

TEST_F(DOMAgentTest, ViewRearranged) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));

  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view = new views::View;
  views::View* target_view = new views::View;
  views::View* child_view = new views::View;
  views::View* child_view_1 = new views::View;

  root_view->AddChildView(parent_view);
  root_view->AddChildView(target_view);
  parent_view->AddChildView(child_view);
  parent_view->AddChildView(child_view_1);

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  // Reorder child_view_1 from index 1 to 0 in view::Views tree. This makes
  // DOM tree remove view node at position 1 and insert it at position 0.
  int child_1_id = GetIDForBackendElement(child_view_1);
  parent_view->ReorderChildView(child_view_1, 0);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_1_id));
  EXPECT_TRUE(WasChildNodeInserted(parent_view));

  // Reorder child_view_1 to the same index 0 shouldn't perform reroder
  // work, so we still expect 1 remove and 1 insert protocol notification
  // messages.
  parent_view->ReorderChildView(child_view_1, 0);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_1_id));
  EXPECT_TRUE(WasChildNodeInserted(parent_view));

  int child_id = GetIDForBackendElement(child_view);
  target_view->AddChildView(child_view);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_id));
  EXPECT_TRUE(WasChildNodeInserted(target_view));
}

TEST_F(DOMAgentTest, ViewRearrangedRemovedAndInserted) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 80, 80)));

  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view = new views::View;
  views::View* target_view = new views::View;
  views::View* child_view = new views::View;
  root_view->AddChildView(parent_view);
  root_view->AddChildView(target_view);
  parent_view->AddChildView(child_view);

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int child_id = GetIDForBackendElement(child_view);
  parent_view->RemoveChildView(child_view);
  target_view->AddChildView(child_view);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_id));
  EXPECT_TRUE(WasChildNodeInserted(target_view));
}

}  // namespace ui_devtools
