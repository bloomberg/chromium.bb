// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
#define CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_

#include <map>

#include "base/component_export.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/caption_buttons/frame_size_button_delegate.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/window/frame_caption_button.h"

namespace gfx {
class SlideAnimation;
struct VectorIcon;
}  // namespace gfx

namespace views {
class Widget;
}

namespace chromeos {

// Container view for the frame caption buttons. It performs the appropriate
// action when a caption button is clicked.
//
// NOTE: The associated test (frame_caption_button_container_view_unittest.cc)
// is in //ash because it needs ash test support (AshTestBase and its
// utilities).
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameCaptionButtonContainerView
    : public views::BoxLayoutView,
      public FrameSizeButtonDelegate,
      public views::AnimationDelegateViews {
 public:
  METADATA_HEADER(FrameCaptionButtonContainerView);

  // `frame` is the views::Widget that the caption buttons act on.
  // `custom_button` is an optional caption button. It is placed as the
  // left-most caption button (in LTR mode).
  FrameCaptionButtonContainerView(
      views::Widget* frame,
      std::unique_ptr<views::FrameCaptionButton> custom_button = nullptr);
  FrameCaptionButtonContainerView(const FrameCaptionButtonContainerView&) =
      delete;
  FrameCaptionButtonContainerView& operator=(
      const FrameCaptionButtonContainerView&) = delete;
  ~FrameCaptionButtonContainerView() override;

  // For testing.
  class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) TestApi {
   public:
    explicit TestApi(FrameCaptionButtonContainerView* container_view)
        : container_view_(container_view) {}
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void EndAnimations();

    views::FrameCaptionButton* minimize_button() const {
      return container_view_->minimize_button_;
    }

    views::FrameCaptionButton* size_button() const {
      return container_view_->size_button_;
    }

    views::FrameCaptionButton* close_button() const {
      return container_view_->close_button_;
    }

    views::FrameCaptionButton* menu_button() const {
      return container_view_->menu_button_;
    }

    views::FrameCaptionButton* custom_button() const {
      return container_view_->custom_button_;
    }

   private:
    FrameCaptionButtonContainerView* container_view_;
  };

  // Sets the id of the vector image to paint the button for |icon|. The
  // FrameCaptionButtonContainerView will keep track of the image to use for
  // |icon| even if none of the buttons currently use |icon|.
  void SetButtonImage(views::CaptionButtonIcon icon,
                      const gfx::VectorIcon& icon_definition);

  // Sets whether the buttons should be painted as active. Does not schedule
  // a repaint.
  void SetPaintAsActive(bool paint_as_active);

  // Sets the background frame color that buttons should compute their color
  // respective to.
  void SetBackgroundColor(SkColor background_color);

  // Tell the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Creates or removes a layer for the caption button container when window
  // controls overlay is enabled or disabled.
  void OnWindowControlsOverlayEnabledChanged(bool enabled,
                                             SkColor background_color);

  // Updates the caption buttons' state based on the caption button model's
  // state. A parent view should relayout to reflect the change in states.
  void UpdateCaptionButtonState(bool animate);

  void UpdateSizeButtonTooltip(bool use_restore_frame);

  // Sets the size of the buttons in this container.
  void SetButtonSize(const gfx::Size& size);

  // Sets the CaptionButtonModel. Caller is responsible for updating
  // the state by calling UpdateCaptionButtonState.
  void SetModel(std::unique_ptr<CaptionButtonModel> model);
  const CaptionButtonModel* model() const { return model_.get(); }

  // Sets the callback that will be invoked when any size button is pressed. If
  // the callback is set, the default behavior (e.g. maximize |frame_|) will be
  // skipped so caller must be responsible for the action. If the callback
  // returns false, it will fall back to the default dehavior.
  void SetOnSizeButtonPressedCallback(base::RepeatingCallback<bool()> callback);
  void ClearOnSizeButtonPressedCallback();

  // views::View:
  void Layout() override;
  void ChildPreferredSizeChanged(View* child) override;
  void ChildVisibilityChanged(View* child) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  friend class FrameCaptionButtonContainerViewTest;

  // Sets |button|'s icon to |icon|. If |animate| is Animate::kYes, the button
  // will crossfade to the new icon. If |animate| is Animate::kNo and
  // |icon| == |button|->icon(), the crossfade animation is progressed to the
  // end.
  void SetButtonIcon(views::FrameCaptionButton* button,
                     views::CaptionButtonIcon icon,
                     Animate animate);

  void MinimizeButtonPressed();
  void SizeButtonPressed();
  void CloseButtonPressed();
  void MenuButtonPressed();

  // FrameSizeButtonDelegate:
  bool IsMinimizeButtonVisible() const override;
  void SetButtonsToNormal(Animate animate) override;
  void SetButtonIcons(views::CaptionButtonIcon minimize_button_icon,
                      views::CaptionButtonIcon close_button_icon,
                      Animate animate) override;
  const views::FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const override;
  void SetHoveredAndPressedButtons(
      const views::FrameCaptionButton* to_hover,
      const views::FrameCaptionButton* to_press) override;
  bool CanSnap() override;
  void ShowSnapPreview(SnapDirection snap, bool allow_haptic_feedback) override;
  void CommitSnap(SnapDirection snap) override;

  // The widget that the buttons act on.
  views::Widget* frame_;

  // The buttons. In the normal button style, at most one of |minimize_button_|
  // and |size_button_| is visible.
  views::FrameCaptionButton* custom_button_ = nullptr;
  views::FrameCaptionButton* menu_button_ = nullptr;
  views::FrameCaptionButton* minimize_button_ = nullptr;
  views::FrameCaptionButton* size_button_ = nullptr;
  views::FrameCaptionButton* close_button_ = nullptr;

  // Mapping of the image needed to paint a button for each of the values of
  // CaptionButtonIcon.
  std::map<views::CaptionButtonIcon, const gfx::VectorIcon*> button_icon_map_;

  // Animation that affects the visibility of |size_button_| and the position of
  // buttons to the left of it. Usually this is just the minimize button but it
  // can also include a PWA menu button.
  std::unique_ptr<gfx::SlideAnimation> tablet_mode_animation_;

  std::unique_ptr<CaptionButtonModel> model_;

  // Callback for the size button action, which overrides the default behavior.
  // If the callback returns false, it will fall back to the default dehavior.
  base::RepeatingCallback<bool()> on_size_button_pressed_callback_;

  // Keeps track of the window-controls-overlay toggle, and defines if the
  // background of the entire view should be updated when the background of the
  // button container changes and SetBackgroundColor() gets called.
  bool window_controls_overlay_enabled_ = false;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_CAPTION_BUTTONS_FRAME_CAPTION_BUTTON_CONTAINER_VIEW_H_
