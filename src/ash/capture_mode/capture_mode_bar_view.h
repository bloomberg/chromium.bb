// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Separator;
}  // namespace views

namespace ash {

class CaptureModeButton;
class CaptureModeSourceView;
class CaptureModeToggleButton;
class CaptureModeTypeView;

// A view that acts as the content view of the capture mode bar widget.
// It has a set of buttons to toggle between image and video capture, and
// another set of buttons to toggle between fullscreen, region, and window
// capture sources. It also contains a settings button. The structure looks like
// this:
//
//   +---------------------------------------------------------------+
//   |  +----------------+  |                       |                |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  |  |   |  |   |  |  |  |   |  |   |  |   |  |  |   |  |   |  |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  +----------------+  |  ^                 ^  |  ^      ^      |
//   +--^----------------------|-----------------|-----|------|------+
//   ^  |                      +-----------------+     |      |
//   |  |                      |                       |      CaptureModeButton
//   |  |                      |                       |
//   |  |                      |                       CaptureModeToggleButton
//   |  |                      CaptureModeSourceView
//   |  CaptureModeTypeView
//   |
//   CaptureModeBarView
//
class ASH_EXPORT CaptureModeBarView : public views::View {
 public:
  METADATA_HEADER(CaptureModeBarView);

  // |projector_mode| is true when the current session was started through the
  // projector workflow.
  explicit CaptureModeBarView(bool projector_mode);
  CaptureModeBarView(const CaptureModeBarView&) = delete;
  CaptureModeBarView& operator=(const CaptureModeBarView&) = delete;
  ~CaptureModeBarView() override;

  CaptureModeTypeView* capture_type_view() const { return capture_type_view_; }
  CaptureModeSourceView* capture_source_view() const {
    return capture_source_view_;
  }
  CaptureModeToggleButton* settings_button() const { return settings_button_; }
  CaptureModeButton* close_button() const { return close_button_; }

  // Gets the ideal bounds in screen coordinates of the bar of widget on the
  // given `root` window. The `image_toggle_button` will not be shown in the bar
  // if `is_in_projector_mode` is true, which means the width of the bar will be
  // different.
  static gfx::Rect GetBounds(aura::Window* root, bool is_in_projector_mode);

  // Called when either the capture mode source or type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);

  // Called when settings is toggled on or off.
  void SetSettingsMenuShown(bool shown);

 private:
  void OnSettingsButtonPressed();
  void OnCloseButtonPressed();

  // Owned by the views hierarchy.
  CaptureModeTypeView* capture_type_view_;
  views::Separator* separator_1_;
  CaptureModeSourceView* capture_source_view_;
  views::Separator* separator_2_;
  CaptureModeToggleButton* settings_button_;
  CaptureModeButton* close_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
