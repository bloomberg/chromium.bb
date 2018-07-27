// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_TEST_WS_TEST_DRAG_DROP_CLIENT_H_
#define SERVICES_UI_TEST_WS_TEST_DRAG_DROP_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ui {
namespace test {

// Drives drag and drop loop in a single root window for test. The logic is
// based on ash::DragDropController.
class TestDragDropClient : public aura::client::DragDropClient,
                           public aura::WindowObserver,
                           public ui::EventHandler {
 public:
  TestDragDropClient();
  ~TestDragDropClient() override;

  // aura::client::DragDropClient
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::Window* root_window,
                       aura::Window* source_window,
                       const gfx::Point& screen_location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) override;
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override {}
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
  }

  // aura::WindowObserer
  void OnWindowDestroyed(aura::Window* window) override;

  // ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  void DragUpdate(aura::Window* target, const ui::LocatedEvent& event);
  void Drop(aura::Window* target, const ui::LocatedEvent& event);

  // The root window where drag and drop happens.
  aura::Window* root_window_ = nullptr;

  // The window that is under the drag cursor.
  aura::Window* drag_window_ = nullptr;

  const ui::OSExchangeData* drag_data_ = nullptr;
  int drag_operation_ = ui::DragDropTypes::DRAG_NONE;

  std::unique_ptr<base::RunLoop> drag_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

}  // namespace test
}  // namespace ui

#endif  // SERVICES_UI_TEST_WS_TEST_DRAG_DROP_CLIENT_H_
