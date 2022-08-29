// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"

class Browser;
class ReadAnythingController;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator
//
//  A class that coordinates the Read Anything feature. This class registers
//  itself as a SidePanelEntry. It creates and owns the Read Anything controller
//  and model. It also creates the Read Anything views when requested by the
//  Side Panel controller.
//  The coordinator acts as the external-facing API for the Read Anything
//  feature. Classes outside this feature should make calls to the coordinator.
//  This class has the same lifetime as the browser.
//
class ReadAnythingCoordinator
    : public BrowserUserData<ReadAnythingCoordinator> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCoordinatorDestroyed() = 0;
  };

  explicit ReadAnythingCoordinator(Browser* browser);
  ReadAnythingCoordinator(const ReadAnythingCoordinator&) = delete;
  ReadAnythingCoordinator& operator=(const ReadAnythingCoordinator&) = delete;
  ~ReadAnythingCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  ReadAnythingController* GetController();
  ReadAnythingModel* GetModel();

  void AddObserver(ReadAnythingCoordinator::Observer* observer);
  void RemoveObserver(ReadAnythingCoordinator::Observer* observer);

 private:
  friend class BrowserUserData<ReadAnythingCoordinator>;
  friend class ReadAnythingCoordinatorTest;

  // Callback passed to SidePanelCoordinator. This function creates the
  // container view and all its child views and returns it.
  std::unique_ptr<views::View> CreateContainerView();

  std::unique_ptr<ReadAnythingModel> model_;
  std::unique_ptr<ReadAnythingController> controller_;

  base::ObserverList<Observer> observers_;
  BROWSER_USER_DATA_KEY_DECL();
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
