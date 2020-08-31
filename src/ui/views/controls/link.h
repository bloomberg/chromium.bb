// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_LINK_H_
#define UI_VIEWS_CONTROLS_LINK_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"

namespace views {

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
  METADATA_HEADER(Link);

  // A callback to be called when the link is clicked.  Closures are also
  // accepted; see below.
  using ClickedCallback =
      base::RepeatingCallback<void(Link* source, int event_flags)>;

  explicit Link(const base::string16& title,
                int text_context = style::CONTEXT_LABEL,
                int text_style = style::STYLE_LINK);
  ~Link() override;

  // Allow providing callbacks that expect either zero or two args, since many
  // callers don't care about the arguments and can avoid adapter functions this
  // way.
  void set_callback(base::RepeatingClosure callback) {
    // Adapt this closure to a ClickedCallback by discarding the extra args.
    callback_ = base::BindRepeating(
        [](base::RepeatingClosure closure, Link*, int) { closure.Run(); },
        std::move(callback));
  }
  void set_callback(ClickedCallback callback) {
    callback_ = std::move(callback);
  }

  SkColor GetColor() const;

  // Label:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  bool CanProcessEventsWithinSubtree() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;
  void SetFontList(const gfx::FontList& font_list) override;
  void SetText(const base::string16& text) override;
  void OnThemeChanged() override;
  void SetEnabledColor(SkColor color) override;
  bool IsSelectionSupported() const override;

 private:
  void SetPressed(bool pressed);

  void RecalculateFont();

  void ConfigureFocus();

  ClickedCallback callback_;

  // Whether the link is currently pressed.
  bool pressed_ = false;

  // The color when the link is neither pressed nor disabled.
  base::Optional<SkColor> requested_enabled_color_;

  PropertyChangedSubscription enabled_changed_subscription_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_LINK_H_
