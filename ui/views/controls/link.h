// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_H_
#define UI_VIEWS_CONTROLS_LINK_H_
#pragma once

#include <string>

#include "ui/views/controls/label.h"

namespace views {

class LinkListener;

////////////////////////////////////////////////////////////////////////////////
//
// Link class
//
// A Link is a label subclass that looks like an HTML link. It has a
// controller which is notified when a click occurs.
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT Link : public Label {
 public:
  Link();
  explicit Link(const string16& title);
  virtual ~Link();

  const LinkListener* listener() { return listener_; }
  void set_listener(LinkListener* listener) { listener_ = listener; }

  // Overridden from View:
  virtual void OnEnabledChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const MouseEvent& event) OVERRIDE;
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from Label:
  virtual void SetFont(const gfx::Font& font) OVERRIDE;

  virtual void SetEnabledColor(const SkColor& color) OVERRIDE;
  void SetPressedColor(const SkColor& color);

  static const char kViewClassName[];

 private:
  void Init();

  void SetPressed(bool pressed);

  void RecalculateFont();

  LinkListener* listener_;

  // Whether the link is currently pressed.
  bool pressed_;

  // The color when the link is neither pressed nor disabled.
  SkColor requested_enabled_color_;

  // The color when the link is pressed.
  SkColor requested_pressed_color_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_H_
