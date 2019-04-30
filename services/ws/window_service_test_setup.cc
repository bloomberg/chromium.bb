// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_service_test_setup.h"

#include "base/bind.h"
#include "services/ws/embedding.h"
#include "services/ws/event_queue.h"
#include "services/ws/event_queue_test_helper.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_binding.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/display/screen.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ws {
namespace {

class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;
  ~TestFocusRules() override = default;

  // wm::BaseFocusRules:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return window == window->GetRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

// An EventSource that mimics AshWindowTreeHostPlatform's EventQueue usage.
class EventSourceWithQueue : public ui::EventSource {
 public:
  explicit EventSourceWithQueue(WindowServiceTestSetup* test_setup,
                                aura::Window* root)
      : test_setup_(test_setup), root_(root) {}
  ~EventSourceWithQueue() override = default;

  // ui::EventSource:
  ui::EventSink* GetEventSink() override {
    return root_->GetHost()->GetEventSink();
  }
  ui::EventDispatchDetails DeliverEventToSink(ui::Event* event) override {
    auto* queue = test_setup_->service()->event_queue();
    // Queue the event if needed, or deliver it directly to the sink.
    auto result = queue->DeliverOrQueueEvent(root_->GetHost(), event);
    if (test_setup_->ack_events_immediately() &&
        EventQueueTestHelper(queue).HasInFlightEvent()) {
      EventQueueTestHelper(queue).AckInFlightEvent();
    }

    return result.value_or(ui::EventDispatchDetails());
  }

 private:
  WindowServiceTestSetup* test_setup_;
  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(EventSourceWithQueue);
};

// EventGeneratorDelegate implementation for mus.
class EventGeneratorDelegateWs : public aura::test::EventGeneratorDelegateAura {
 public:
  explicit EventGeneratorDelegateWs(WindowServiceTestSetup* test_setup,
                                    aura::Window* root)
      : root_(root), event_source_(test_setup, root) {}
  ~EventGeneratorDelegateWs() override = default;

  // EventGeneratorDelegateAura:
  ui::EventTarget* GetTargetAt(const gfx::Point& location) override {
    return root_;
  }
  ui::EventSource* GetEventSource(ui::EventTarget* target) override {
    return target == root_ ? &event_source_
                           : EventGeneratorDelegateAura::GetEventSource(target);
  }
  gfx::Point CenterOfTarget(const ui::EventTarget* target) const override {
    if (target != root_)
      return EventGeneratorDelegateAura::CenterOfTarget(target);
    return display::Screen::GetScreen()
        ->GetPrimaryDisplay()
        .bounds()
        .CenterPoint();
  }
  void ConvertPointFromTarget(const ui::EventTarget* target,
                              gfx::Point* point) const override {
    if (target != root_)
      EventGeneratorDelegateAura::ConvertPointFromTarget(target, point);
  }
  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {
    if (target != root_)
      EventGeneratorDelegateAura::ConvertPointToTarget(target, point);
  }
  void ConvertPointFromHost(const ui::EventTarget* hosted_target,
                            gfx::Point* point) const override {
    if (hosted_target != root_)
      EventGeneratorDelegateAura::ConvertPointFromHost(hosted_target, point);
  }

 private:
  aura::Window* root_;
  EventSourceWithQueue event_source_;

  DISALLOW_COPY_AND_ASSIGN(EventGeneratorDelegateWs);
};

std::unique_ptr<ui::test::EventGeneratorDelegate> CreateEventGeneratorDelegate(
    WindowServiceTestSetup* test_setup,
    ui::test::EventGenerator* owner,
    aura::Window* root_window,
    aura::Window* window) {
  DCHECK(root_window);
  DCHECK(root_window->GetHost());
  return std::make_unique<EventGeneratorDelegateWs>(test_setup, root_window);
}

}  // namespace

WindowServiceTestSetup::WindowServiceTestSetup()
    // FocusController takes ownership of TestFocusRules.
    : focus_controller_(new TestFocusRules()) {
  DCHECK_EQ(gl::kGLImplementationNone, gl::GetGLImplementation());
  gl::GLSurfaceTestSupport::InitializeOneOff();

  const bool enable_pixel_output = false;
  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output);
  aura_test_helper_.SetUp(context_factories_->GetContextFactory(),
                          context_factories_->GetContextFactoryPrivate());
  // The resize throttle may interfere with tests, so disable it. If specific
  // tests want the throttle, they can enable it.
  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
  scoped_capture_client_ = std::make_unique<wm::ScopedCaptureClient>(
      aura_test_helper_.root_window());
  service_ =
      std::make_unique<WindowService>(&delegate_, nullptr, focus_controller());
  aura::client::SetFocusClient(root(), focus_controller());
  wm::SetActivationClient(root(), focus_controller());
  delegate_.set_top_level_parent(aura_test_helper_.root_window());

  window_tree_ = service_->CreateWindowTree(&window_tree_client_);
  window_tree_->InitFromFactory();
  window_tree_test_helper_ =
      std::make_unique<WindowTreeTestHelper>(window_tree_.get());

  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&CreateEventGeneratorDelegate, this));
}

WindowServiceTestSetup::~WindowServiceTestSetup() {
  window_tree_test_helper_.reset();
  window_tree_.reset();
  service_.reset();
  scoped_capture_client_.reset();
  aura::client::SetFocusClient(root(), nullptr);
  aura_test_helper_.TearDown();
  context_factories_.reset();
  gl::GLSurfaceTestSupport::ShutdownGL();
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());
}

std::unique_ptr<EmbeddingHelper> WindowServiceTestSetup::CreateEmbedding(
    aura::Window* embed_root,
    uint32_t flags) {
  auto embedding_helper = std::make_unique<EmbeddingHelper>();
  embedding_helper->embedding = window_tree_test_helper_->Embed(
      embed_root, nullptr, &embedding_helper->window_tree_client, flags);
  if (!embedding_helper->embedding)
    return nullptr;
  embedding_helper->window_tree = embedding_helper->embedding->embedded_tree();
  embedding_helper->window_tree_test_helper =
      std::make_unique<WindowTreeTestHelper>(embedding_helper->window_tree);
  embedding_helper->parent_window_tree =
      embedding_helper->embedding->embedding_tree();
  return embedding_helper;
}

EmbeddingHelper::EmbeddingHelper() = default;

EmbeddingHelper::~EmbeddingHelper() {
  if (!embedding)
    return;

  WindowTreeTestHelper(parent_window_tree).DestroyEmbedding(embedding);
}

}  // namespace ws
