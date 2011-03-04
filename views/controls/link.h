// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_LINK_H_
#define VIEWS_CONTROLS_LINK_H_
#pragma once

#include <string>

#include "views/controls/label.h"

namespace views {

class Link;

////////////////////////////////////////////////////////////////////////////////
//
// LinkController defines the method that should be implemented to
// receive a notification when a link is clicked
//
////////////////////////////////////////////////////////////////////////////////
class LinkController {
 public:
  virtual void LinkActivated(Link* source, int event_flags) = 0;

 protected:
  virtual ~LinkController() {}
};

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

  void SetController(LinkController* controller);
  const LinkController* GetController();

  // Overridden from View:
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event,
                               bool canceled);
  virtual bool OnKeyPressed(const KeyEvent& e);
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e);

  // Accessibility accessors, overridden from View:
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual void SetFont(const gfx::Font& font);

  // Set whether the link is enabled.
  virtual void SetEnabled(bool f);

  virtual gfx::NativeCursor GetCursorForPoint(ui::EventType event_type,
                                              const gfx::Point& p);

  virtual std::string GetClassName() const;

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

  LinkController* controller_;

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
