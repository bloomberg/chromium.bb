// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_
#define UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_

#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace ui {

// Bound of a selected region.
struct UI_BASE_EXPORT SelectionBound {
 public:
  enum Type {
    LEFT,
    RIGHT,
    CENTER,
    EMPTY,
    LAST = EMPTY
  };

  SelectionBound();
  ~SelectionBound();

  int GetHeight() const;

  Type type;

  gfx::Point edge_top;
  gfx::Point edge_bottom;
};

UI_BASE_EXPORT bool operator==(const SelectionBound& lhs,
                               const SelectionBound& rhs);
UI_BASE_EXPORT bool operator!=(const SelectionBound& lhs,
                               const SelectionBound& rhs);

UI_BASE_EXPORT gfx::Rect RectBetweenSelectionBounds(const SelectionBound& b1,
                                                    const SelectionBound& b2);

// An interface implemented by widget that has text that can be selected/edited
// using touch.
class UI_BASE_EXPORT TouchEditable : public ui::SimpleMenuModel::Delegate {
 public:
  // TODO(mohsen): Consider switching from local coordinates to screen
  // coordinates in this interface and see if it will simplify things.

  // Select everything between start and end (points are in view's local
  // coordinate system). |start| is the logical start and |end| is the logical
  // end of selection. Visually, |start| may lie after |end|.
  virtual void SelectRect(const gfx::Point& start, const gfx::Point& end) = 0;

  // Move the caret to |point|. |point| is in local coordinates.
  virtual void MoveCaretTo(const gfx::Point& point) = 0;

  // Gets the end points of the current selection. The end points |anchor| and
  // |focus| must be the cursor rect for the logical start and logical end of
  // selection (in local coordinates):
  // ____________________________________
  // | textfield with |selected text|   |
  // ------------------------------------
  //                  ^anchor       ^focus
  //
  // Visually, anchor could be to the right of focus in the figure above - it
  // depends on the selection direction.
  virtual void GetSelectionEndPoints(ui::SelectionBound* anchor,
                                     ui::SelectionBound* focus) = 0;

  // Gets the bounds of the client view in its local coordinates.
  virtual gfx::Rect GetBounds() = 0;

  // Gets the NativeView hosting the client.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Converts a point to/from screen coordinates from/to client view.
  virtual void ConvertPointToScreen(gfx::Point* point) = 0;
  virtual void ConvertPointFromScreen(gfx::Point* point) = 0;

  // Returns true if the editable draws its own handles (hence, the
  // TouchSelectionController need not draw handles).
  virtual bool DrawsHandles() = 0;

  // Tells the editable to open context menu.
  virtual void OpenContextMenu(const gfx::Point& anchor) = 0;

  // Tells the editable to end touch editing and destroy touch selection
  // controller it owns.
  virtual void DestroyTouchSelection() = 0;

 protected:
  ~TouchEditable() override {}
};

// This defines the callback interface for other code to be notified of changes
// in the state of a TouchEditable.
class UI_BASE_EXPORT TouchSelectionController {
 public:
  virtual ~TouchSelectionController() {}

  // Creates a TouchSelectionController. Caller owns the returned object.
  static TouchSelectionController* create(
      TouchEditable* client_view);

  // Notifies the controller that the selection has changed.
  virtual void SelectionChanged() = 0;

  // Returns true if the user is currently dragging one of the handles.
  virtual bool IsHandleDragInProgress() = 0;

  // Hides visible handles. According to the value of |quick|, handles might
  // fade out quickly or slowly.
  virtual void HideHandles(bool quick) = 0;
};

class UI_BASE_EXPORT TouchSelectionControllerFactory {
 public:
  static void SetInstance(TouchSelectionControllerFactory* instance);

  virtual TouchSelectionController* create(TouchEditable* client_view) = 0;

 protected:
  virtual ~TouchSelectionControllerFactory() {}
};

}  // namespace ui

#endif  // UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_
