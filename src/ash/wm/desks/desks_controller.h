// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class Desk;

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class ASH_EXPORT DesksController {
 public:
  class Observer {
   public:
    // Called when |desk| has been created and added to
    // `DesksController::desks_`.
    virtual void OnDeskAdded(const Desk* desk) = 0;

    // Called when |desk| has been removed from `DesksController::desks_`.
    // However |desk| is kept alive temporarily and will be destroyed after all
    // observers have been notified with this.
    virtual void OnDeskRemoved(const Desk* desk) = 0;

   protected:
    virtual ~Observer() = default;
  };

  DesksController();
  ~DesksController();

  static constexpr size_t kMaxNumberOfDesks = 4;

  // Convenience method for returning the DesksController instance. The actual
  // instance is created and owned by Shell.
  static DesksController* Get();

  const std::vector<std::unique_ptr<Desk>>& desks() const { return desks_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if we haven't reached the maximum allowed number of desks.
  bool CanCreateDesks() const;

  // Returns true as long as there are two or more desks. It is required that
  // there is at least one single desk at any time.
  bool CanRemoveDesks() const;

  // Creates a new desk. CanCreateDesks() must be checked before calling this.
  void NewDesk();

  // Removes and deletes the given |desk|. |desk| must already exist, and
  // CanRemoveDesks() must be checked before this.
  void RemoveDesk(const Desk* desk);

 private:
  std::vector<std::unique_ptr<Desk>> desks_;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DesksController);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
