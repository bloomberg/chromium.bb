// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

class RenderViewContextMenuProxy;
class SharedClipboardUiController;

class SharedClipboardContextMenuObserver
    : public RenderViewContextMenuObserver {
 public:
  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(SharedClipboardContextMenuObserver* parent);

    SubMenuDelegate(const SubMenuDelegate&) = delete;
    SubMenuDelegate& operator=(const SubMenuDelegate&) = delete;

    ~SubMenuDelegate() override;

    bool IsCommandIdEnabled(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;

   private:
    const raw_ptr<SharedClipboardContextMenuObserver> parent_;
  };

  explicit SharedClipboardContextMenuObserver(
      RenderViewContextMenuProxy* proxy);

  SharedClipboardContextMenuObserver(
      const SharedClipboardContextMenuObserver&) = delete;
  SharedClipboardContextMenuObserver& operator=(
      const SharedClipboardContextMenuObserver&) = delete;

  ~SharedClipboardContextMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           SingleDevice_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           MultipleDevices_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           MultipleDevices_MoreThanMax_ShowMenu);

  void BuildSubMenu();

  void SendSharedClipboardMessage(int chosen_device_index);

  raw_ptr<RenderViewContextMenuProxy> proxy_ = nullptr;

  raw_ptr<SharedClipboardUiController> controller_ = nullptr;

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices_;

  SubMenuDelegate sub_menu_delegate_{this};

  std::u16string text_;

  std::unique_ptr<ui::SimpleMenuModel> sub_menu_model_;
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_
