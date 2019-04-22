// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/in_flight_change.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace aura {

std::string ChangeTypeToString(ChangeType change_type) {
  switch (change_type) {
    case ChangeType::ADD_CHILD:
      return "ADD_CHILD";
    case ChangeType::ADD_TRANSIENT_WINDOW:
      return "ADD_TRANSIENT_WINDOW";
    case ChangeType::BOUNDS:
      return "BOUNDS";
    case ChangeType::CAPTURE:
      return "CAPTURE";
    case ChangeType::CHILD_MODAL_PARENT:
      return "CHILD_MODAL_PARENT";
    case ChangeType::DELETE_WINDOW:
      return "DELETE_WINDOW";
    case ChangeType::DRAG_LOOP:
      return "DRAG_LOOP";
    case ChangeType::FOCUS:
      return "FOCUS";
    case ChangeType::MOVE_LOOP:
      return "MOVE_LOOP";
    case ChangeType::NEW_TOP_LEVEL_WINDOW:
      return "NEW_TOP_LEVEL_WINDOW";
    case ChangeType::NEW_WINDOW:
      return "NEW_WINDOW";
    case ChangeType::OPACITY:
      return "OPACITY";
    case ChangeType::CURSOR:
      return "CURSOR";
    case ChangeType::PROPERTY:
      return "PROPERTY";
    case ChangeType::REMOVE_CHILD:
      return "REMOVE_CHILD";
    case ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT:
      return "REMOVE_TRANSIENT_WINDOW_FROM_PARENT";
    case ChangeType::REORDER:
      return "REORDER";
    case ChangeType::SET_MODAL:
      return "SET_MODAL";
    case ChangeType::TRANSFORM:
      return "TRANSFORM";
    case ChangeType::VISIBLE:
      return "VISIBLE";
    case ChangeType::SET_TRANSPARENT:
      return "SET_TRANSPARENT";
  }
}

// InFlightChange -------------------------------------------------------------

InFlightChange::InFlightChange(WindowMus* window, ChangeType type)
    : window_(window), change_type_(type) {}

InFlightChange::~InFlightChange() {}

bool InFlightChange::Matches(const InFlightChange& change) const {
  DCHECK(change.window_ == window_ && change.change_type_ == change_type_);
  return true;
}

void InFlightChange::ChangeFailed() {}

void InFlightChange::OnLastChangeOfTypeSucceeded() {}

// InFlightBoundsChange -------------------------------------------------------

InFlightBoundsChange::InFlightBoundsChange(
    WindowTreeClient* window_tree_client,
    WindowMus* window,
    const gfx::Rect& revert_bounds,
    ui::WindowShowState revert_show_state,
    bool from_server,
    const base::Optional<viz::LocalSurfaceIdAllocation>&
        revert_local_surface_id_allocation)
    : InFlightChange(window, ChangeType::BOUNDS),
      window_tree_client_(window_tree_client),
      revert_bounds_(revert_bounds),
      revert_show_state_(revert_show_state),
      from_server_(from_server),
      revert_local_surface_id_allocation_(revert_local_surface_id_allocation) {
  // Should always be created with a window.
  CHECK(window);
}

InFlightBoundsChange::~InFlightBoundsChange() {}

void InFlightBoundsChange::SetRevertValueFrom(const InFlightChange& change) {
  const auto& from = static_cast<const InFlightBoundsChange&>(change);
  from_server_ = from.from_server_;
  revert_bounds_ = from.revert_bounds_;
  revert_show_state_ = from.revert_show_state_;
  revert_local_surface_id_allocation_ =
      from.revert_local_surface_id_allocation_;
}

void InFlightBoundsChange::Revert() {
  window_tree_client_->SetWindowBoundsFromServer(
      window(), revert_bounds_, revert_show_state_, from_server_,
      revert_local_surface_id_allocation_);
}

void InFlightBoundsChange::OnLastChangeOfTypeSucceeded() {
  // InFlightBoundsChange is always created with a window.
  CHECK(window());
  aura::Window* w = window()->GetWindow();
  // WindowTreeHostMus returns null if not a WindowTreeHostMus.
  aura::WindowTreeHostMus* window_tree_host = WindowTreeHostMus::ForWindow(w);
  // ApplyPendingSurfaceIdFromServer() is only applicable for
  // WindowTreeHostMus's window(). If |w| is not that window, nothing to do.
  if (!window_tree_host || w != window_tree_host->window() ||
      !window_tree_host->has_pending_local_surface_id_from_server()) {
    return;
  }
  window_tree_client_->ApplyPendingSurfaceIdFromServer(window());
}

// InFlightDragChange -----------------------------------------------------

InFlightDragChange::InFlightDragChange(WindowMus* window, ChangeType type)
    : InFlightChange(window, type) {
  DCHECK(type == ChangeType::MOVE_LOOP || type == ChangeType::DRAG_LOOP);
}

void InFlightDragChange::SetRevertValueFrom(const InFlightChange& change) {}

void InFlightDragChange::Revert() {}

// InFlightTransformChange -----------------------------------------------------

InFlightTransformChange::InFlightTransformChange(
    WindowTreeClient* window_tree_client,
    WindowMus* window,
    const gfx::Transform& revert_transform)
    : InFlightChange(window, ChangeType::TRANSFORM),
      window_tree_client_(window_tree_client),
      revert_transform_(revert_transform) {}

InFlightTransformChange::~InFlightTransformChange() {}

void InFlightTransformChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_transform_ =
      static_cast<const InFlightTransformChange&>(change).revert_transform_;
}

void InFlightTransformChange::Revert() {
  window_tree_client_->SetWindowTransformFromServer(window(),
                                                    revert_transform_);
}

// CrashInFlightChange --------------------------------------------------------

CrashInFlightChange::CrashInFlightChange(const base::Location& from_here,
                                         WindowMus* window,
                                         ChangeType type)
    : InFlightChange(window, type), location_(from_here) {}

CrashInFlightChange::~CrashInFlightChange() {}

void CrashInFlightChange::SetRevertValueFrom(const InFlightChange& change) {
  CHECK(false);
}

void CrashInFlightChange::ChangeFailed() {
  // TODO(crbug.com/912228, crbug.com/940620): remove LOG(). Used to figure out
  // why this is being hit.
  LOG(FATAL) << "change failed, type=" << ChangeTypeToString(change_type())
             << "(" << static_cast<int>(change_type()) << ")"
             << ", window="
             << (window() ? window()->GetWindow()->GetName() : "(null)")
             << ", parent="
             << (window() && window()->GetWindow()->parent()
                     ? window()->GetWindow()->parent()->GetName()
                     : "(null)")
             << ", from=" << location_.ToString();
}

void CrashInFlightChange::Revert() {
  CHECK(false);
}

// InFlightWindowChange -------------------------------------------------------

InFlightWindowTreeClientChange::InFlightWindowTreeClientChange(
    WindowTreeClient* client,
    WindowMus* revert_value,
    ChangeType type)
    : InFlightChange(nullptr, type), client_(client), revert_window_(nullptr) {
  SetRevertWindow(revert_value);
}

InFlightWindowTreeClientChange::~InFlightWindowTreeClientChange() {
  SetRevertWindow(nullptr);
}

void InFlightWindowTreeClientChange::SetRevertValueFrom(
    const InFlightChange& change) {
  SetRevertWindow(static_cast<const InFlightWindowTreeClientChange&>(change)
                      .revert_window_);
}

void InFlightWindowTreeClientChange::SetRevertWindow(WindowMus* window) {
  if (revert_window_)
    revert_window_->GetWindow()->RemoveObserver(this);
  revert_window_ = window;
  if (revert_window_)
    revert_window_->GetWindow()->AddObserver(this);
}

void InFlightWindowTreeClientChange::OnWindowDestroyed(Window* window) {
  // NOTE: this has to be in OnWindowDestroyed() as FocusClients typically
  // change focus in OnWindowDestroying().
  SetRevertWindow(nullptr);
}

// InFlightCaptureChange ------------------------------------------------------

InFlightCaptureChange::InFlightCaptureChange(
    WindowTreeClient* client,
    CaptureSynchronizer* capture_synchronizer,
    WindowMus* revert_value)
    : InFlightWindowTreeClientChange(client, revert_value, ChangeType::CAPTURE),
      capture_synchronizer_(capture_synchronizer) {}

InFlightCaptureChange::~InFlightCaptureChange() {}

void InFlightCaptureChange::Revert() {
  capture_synchronizer_->SetCaptureFromServer(revert_window());
}

// InFlightFocusChange --------------------------------------------------------

InFlightFocusChange::InFlightFocusChange(WindowTreeClient* client,
                                         FocusSynchronizer* focus_synchronizer,
                                         WindowMus* revert_value)
    : InFlightWindowTreeClientChange(client, revert_value, ChangeType::FOCUS),
      focus_synchronizer_(focus_synchronizer) {}

InFlightFocusChange::~InFlightFocusChange() {}

void InFlightFocusChange::Revert() {
  focus_synchronizer_->SetFocusFromServer(revert_window());
}

// InFlightPropertyChange -----------------------------------------------------

InFlightPropertyChange::InFlightPropertyChange(
    WindowMus* window,
    const std::string& property_name,
    std::unique_ptr<std::vector<uint8_t>> revert_value)
    : InFlightChange(window, ChangeType::PROPERTY),
      property_name_(property_name),
      revert_value_(std::move(revert_value)) {}

InFlightPropertyChange::~InFlightPropertyChange() {}

bool InFlightPropertyChange::Matches(const InFlightChange& change) const {
  return static_cast<const InFlightPropertyChange&>(change).property_name_ ==
         property_name_;
}

void InFlightPropertyChange::SetRevertValueFrom(const InFlightChange& change) {
  const InFlightPropertyChange& property_change =
      static_cast<const InFlightPropertyChange&>(change);
  if (property_change.revert_value_) {
    revert_value_ =
        std::make_unique<std::vector<uint8_t>>(*property_change.revert_value_);
  } else {
    revert_value_.reset();
  }
}

void InFlightPropertyChange::Revert() {
  window()->SetPropertyFromServer(property_name_, revert_value_.get());
}

// InFlightCursorChange ----------------------------------------------------

InFlightCursorChange::InFlightCursorChange(WindowMus* window,
                                           const ui::Cursor& revert_value)
    : InFlightChange(window, ChangeType::CURSOR),
      revert_cursor_(revert_value) {}

InFlightCursorChange::~InFlightCursorChange() {}

void InFlightCursorChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_cursor_ =
      static_cast<const InFlightCursorChange&>(change).revert_cursor_;
}

void InFlightCursorChange::Revert() {
  window()->SetCursorFromServer(revert_cursor_);
}

// InFlightVisibleChange -------------------------------------------------------

InFlightVisibleChange::InFlightVisibleChange(WindowTreeClient* client,
                                             WindowMus* window,
                                             bool revert_value)
    : InFlightChange(window, ChangeType::VISIBLE),
      window_tree_client_(client),
      revert_visible_(revert_value) {}

InFlightVisibleChange::~InFlightVisibleChange() {}

void InFlightVisibleChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_visible_ =
      static_cast<const InFlightVisibleChange&>(change).revert_visible_;
}

void InFlightVisibleChange::Revert() {
  window_tree_client_->SetWindowVisibleFromServer(window(), revert_visible_);
}

// InFlightSetModalTypeChange
// ------------------------------------------------------

InFlightSetModalTypeChange::InFlightSetModalTypeChange(
    WindowMus* window,
    ui::ModalType revert_value)
    : InFlightChange(window, ChangeType::SET_MODAL),
      revert_modal_type_(revert_value) {}

InFlightSetModalTypeChange::~InFlightSetModalTypeChange() {}

void InFlightSetModalTypeChange::SetRevertValueFrom(
    const InFlightChange& change) {
  revert_modal_type_ =
      static_cast<const InFlightSetModalTypeChange&>(change).revert_modal_type_;
}

void InFlightSetModalTypeChange::Revert() {
  window()->GetWindow()->SetProperty(client::kModalKey, revert_modal_type_);
}

}  // namespace aura
