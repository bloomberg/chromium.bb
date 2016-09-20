// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/tests/window_server_test_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/shell/public/cpp/connector.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_host_factory.h"

namespace ui {
namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

}  // namespace

WindowServerTestBase::WindowServerTestBase()
    : most_recent_client_(nullptr),
      window_manager_(nullptr),
      window_manager_delegate_(nullptr),
      window_manager_client_(nullptr) {}

WindowServerTestBase::~WindowServerTestBase() {
  window_tree_clients_.clear();
}

// static
bool WindowServerTestBase::DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

// static
bool WindowServerTestBase::QuitRunLoop() {
  if (!current_run_loop)
    return false;

  current_run_loop->Quit();
  current_run_loop = nullptr;
  return true;
}

void WindowServerTestBase::DeleteWindowTreeClient(
    ui::WindowTreeClient* client) {
  for (auto iter = window_tree_clients_.begin();
       iter != window_tree_clients_.end(); ++iter) {
    if (iter->get() == client) {
      window_tree_clients_.erase(iter);
      return;
    }
  }
  NOTREACHED();
}

void WindowServerTestBase::SetUp() {
  WindowServerServiceTestBase::SetUp();

  std::unique_ptr<WindowTreeClient> window_manager_window_tree_client =
      CreateWindowTreeHost(connector(), this, &host_, this);
  window_manager_ = window_manager_window_tree_client.get();
  window_tree_clients_.insert(std::move(window_manager_window_tree_client));

  ASSERT_TRUE(DoRunLoopWithTimeout());  // RunLoop should be quit by OnEmbed().
}

bool WindowServerTestBase::OnConnect(const shell::Identity& remote_identity,
                                     shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::WindowTreeClient>(this);
  return true;
}

void WindowServerTestBase::OnEmbed(Window* root) {
  most_recent_client_ = root->window_tree();
  EXPECT_TRUE(QuitRunLoop());
  ASSERT_TRUE(window_manager_client_);
  window_manager_client_->AddActivationParent(root);
}

void WindowServerTestBase::OnLostConnection(WindowTreeClient* client) {
  window_tree_client_lost_connection_ = true;
  DeleteWindowTreeClient(client);
}

void WindowServerTestBase::OnEmbedRootDestroyed(Window* root) {
  DeleteWindowTreeClient(root->window_tree());
}

void WindowServerTestBase::OnPointerEventObserved(const ui::PointerEvent& event,
                                                  Window* target) {}

void WindowServerTestBase::SetWindowManagerClient(WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool WindowServerTestBase::OnWmSetBounds(Window* window, gfx::Rect* bounds) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmSetBounds(window, bounds)
             : true;
}

bool WindowServerTestBase::OnWmSetProperty(
    Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmSetProperty(window, name, new_data)
             : true;
}

Window* WindowServerTestBase::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmCreateTopLevelWindow(properties)
             : nullptr;
}

void WindowServerTestBase::OnWmClientJankinessChanged(
    const std::set<Window*>& client_windows,
    bool janky) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmClientJankinessChanged(client_windows, janky);
}

void WindowServerTestBase::OnWmNewDisplay(Window* window,
                                          const display::Display& display) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmNewDisplay(window, display);
}

void WindowServerTestBase::OnWmDisplayRemoved(Window* window) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmDisplayRemoved(window);
}

void WindowServerTestBase::OnWmPerformMoveLoop(
    Window* window,
    mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& on_done) {
  if (window_manager_delegate_) {
    window_manager_delegate_->OnWmPerformMoveLoop(window, source,
                                                  cursor_location, on_done);
  }
}

void WindowServerTestBase::OnWmCancelMoveLoop(Window* window) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmCancelMoveLoop(window);
}

mojom::EventResult WindowServerTestBase::OnAccelerator(uint32_t accelerator_id,
                                                       const ui::Event& event) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnAccelerator(accelerator_id, event)
             : mojom::EventResult::UNHANDLED;
}

void WindowServerTestBase::Create(const shell::Identity& remote_identity,
                                  mojom::WindowTreeClientRequest request) {
  window_tree_clients_.insert(
      base::MakeUnique<WindowTreeClient>(this, nullptr, std::move(request)));
}

}  // namespace ui
