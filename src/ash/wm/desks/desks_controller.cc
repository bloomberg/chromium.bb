// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "base/logging.h"

namespace ash {

DesksController::DesksController() {
  // There's always one default desk.
  NewDesk();
}

DesksController::~DesksController() = default;

// static
constexpr size_t DesksController::kMaxNumberOfDesks;

// static
DesksController* DesksController::Get() {
  return Shell::Get()->desks_controller();
}

void DesksController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesksController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesksController::CanCreateDesks() const {
  // TODO(afakhry): Disable creating new desks in tablet mode.
  return desks_.size() < kMaxNumberOfDesks;
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk() {
  DCHECK(CanCreateDesks());

  desks_.emplace_back(std::make_unique<Desk>());

  for (auto& observer : observers_)
    observer.OnDeskAdded(desks_.back().get());
}

void DesksController::RemoveDesk(const Desk* desk) {
  DCHECK(CanRemoveDesks());

  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  DCHECK(iter != desks_.end());

  // TODO(afakhry):
  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.

  std::unique_ptr<Desk> removed_desk = std::move(*iter);
  desks_.erase(iter);

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());
}

}  // namespace ash
