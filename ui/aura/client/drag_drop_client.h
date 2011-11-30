// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
#define UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/aura/event.h"

namespace ui {
class OSExchangeData;
}

namespace aura {

class Window;

// An interface implemented by an object that controls a drag and drop session.
class AURA_EXPORT DragDropClient {
 public:
  virtual ~DragDropClient() {}

  // Initiates a drag and drop session. Returns the drag operation that was
  // applied at the end of the drag drop session.
  virtual int StartDragAndDrop(const ui::OSExchangeData& data,
                                int operation) = 0;

  // Called when mouse is dragged during a drag and drop.
  virtual void DragUpdate(aura::Window* target, const MouseEvent& event) = 0;

  // Called when mouse is released during a drag and drop.
  virtual void Drop(aura::Window* target, const MouseEvent& event) = 0;

  // Called when a drag and drop session is cancelled.
  virtual void DragCancel() = 0;

  // Returns true if a drag and drop session is in progress.
  virtual bool IsDragDropInProgress() = 0;
};

}  // namespace aura

#endif  // UI_AURA_CLIENT_DRAG_DROP_CLIENT_H_
