// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_LINK_H_
#define VIEWS_CONTROLS_LINK_H_
#pragma once

#include <string>

#include "views/controls/label.h"

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
class Link : public Label {
 public:
  Link();
  explicit Link(const std::wstring& title);
  virtual ~Link();

  const LinkListener* listener() { return listener_; }
  void set_listener(LinkListener* listener) { listener_ = listener; }

  // Overridden from View:
  virtual void OnEnabledChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from Label:
  virtual void SetFont(const gfx::Font& font) OVERRIDE;

  void SetHighlightedColor(const SkColor& color);
  void SetDisabledColor(const SkColor& color);
  void SetNormalColor(const SkColor& color);

  // If you'll be displaying the link over some non-system background color,
  // call this with the relevant color and the link will auto-set its colors to
  // be readable.
  void MakeReadableOverBackgroundColor(const SkColor& color);

  static const char kViewClassName[];

 private:
  // A highlighted link is clicked.
  void SetHighlighted(bool f);

  // Make sure the label style matched the current state.
  void ValidateStyle();

  void Init();

  LinkListener* listener_;

  // Whether the link is currently highlighted.
  bool highlighted_;

  // The color when the link is highlighted.
  SkColor highlighted_color_;

  // The color when the link is disabled.
  SkColor disabled_color_;

  // The color when the link is neither highlighted nor disabled.
  SkColor normal_color_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_LINK_H_
