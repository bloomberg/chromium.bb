// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/base/models/simple_menu_model.h"

namespace crosapi {

// This class is the ash-chrome implementation of the TestController interface.
// This class must only be used from the main thread.
class TestControllerAsh : public mojom::TestController,
                          public CrosapiAsh::TestControllerReceiver {
 public:
  TestControllerAsh();
  TestControllerAsh(const TestControllerAsh&) = delete;
  TestControllerAsh& operator=(const TestControllerAsh&) = delete;
  ~TestControllerAsh() override;

  // CrosapiAsh::TestControllerReceiver:
  void BindReceiver(
      mojo::PendingReceiver<mojom::TestController> receiver) override;

  // crosapi::mojom::TestController:
  void ClickWindow(const std::string& window_id) override;
  void DoesItemExistInShelf(const std::string& item_id,
                            DoesItemExistInShelfCallback callback) override;
  void DoesWindowExist(const std::string& window_id,
                       DoesWindowExistCallback callback) override;
  void EnterOverviewMode(EnterOverviewModeCallback callback) override;
  void ExitOverviewMode(ExitOverviewModeCallback callback) override;
  void EnterTabletMode(EnterTabletModeCallback callback) override;
  void ExitTabletMode(ExitTabletModeCallback callback) override;
  void GetContextMenuForShelfItem(
      const std::string& item_id,
      GetContextMenuForShelfItemCallback callback) override;
  void GetMinimizeOnBackKeyWindowProperty(
      const std::string& window_id,
      GetMinimizeOnBackKeyWindowPropertyCallback cb) override;
  void GetWindowPositionInScreen(const std::string& window_id,
                                 GetWindowPositionInScreenCallback cb) override;
  void PinOrUnpinItemInShelf(const std::string& item_id,
                             bool pin,
                             PinOrUnpinItemInShelfCallback cb) override;
  void SelectItemInShelf(const std::string& item_id,
                         SelectItemInShelfCallback cb) override;
  void SendTouchEvent(const std::string& window_id,
                      mojom::TouchEventType type,
                      uint8_t pointer_id,
                      const gfx::PointF& location_in_window,
                      SendTouchEventCallback cb) override;
  void GetOpenAshBrowserWindows(
      GetOpenAshBrowserWindowsCallback callback) override;
  void CloseAllBrowserWindows(CloseAllBrowserWindowsCallback callback) override;
  void RegisterStandaloneBrowserTestController(
      mojo::PendingRemote<mojom::StandaloneBrowserTestController>) override;
  void TriggerTabScrubbing(float x_offset,
                           TriggerTabScrubbingCallback callback) override;

  mojo::Remote<mojom::StandaloneBrowserTestController>&
  GetStandaloneBrowserTestController() {
    DCHECK(standalone_browser_test_controller_.is_bound());
    return standalone_browser_test_controller_;
  }

 private:
  class OverviewWaiter;

  // Called when a waiter has finished waiting for its event.
  void WaiterFinished(OverviewWaiter* waiter);

  // Called when the lacros test controller was disconnected.
  void OnControllerDisconnected();

  // Called when a ShelfItemDelegate returns its context menu.
  static void OnGetContextMenuForShelfItem(
      GetContextMenuForShelfItemCallback callback,
      std::unique_ptr<ui::SimpleMenuModel> model);

  // Each call to EnterOverviewMode or ExitOverviewMode spawns a waiter for the
  // corresponding event. The waiters are stored in this struct and deleted once
  // the event triggers.
  std::vector<std::unique_ptr<OverviewWaiter>> overview_waiters_;

  // This class supports any number of connections. This allows multiple
  // crosapi clients.
  mojo::ReceiverSet<mojom::TestController> receivers_;

  // Controller to send commands to the connected lacros crosapi client.
  mojo::Remote<mojom::StandaloneBrowserTestController>
      standalone_browser_test_controller_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
