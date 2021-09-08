// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_

#include <cstddef>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class FocusRing;
class HighlightPathGenerator;
class View;
class Widget;
}  // namespace views

namespace ash {

class CaptureModeSession;

// CaptureModeSessionFocusCycler handles the special focus transitions which
// happen between the capture session UI items. These include the capture bar
// buttons, the selection region UI and the capture button.
// TODO(crbug.com/1182456): The selection region UI are drawn directly on a
// layer. We simulate focus by drawing focus rings on the same layer, but this
// is not compatible with accessibility. Investigate using AxVirtualView or
// making the dots actual Views.
class ASH_EXPORT CaptureModeSessionFocusCycler : public views::WidgetObserver {
 public:
  // The different groups which can receive focus during a capture mode session.
  // A group may have multiple items which can receive focus.
  // TODO(crbug.com/1182456): Investigate removing the groups concept and having
  // one flat list.
  enum class FocusGroup {
    kNone = 0,
    // The buttons to select the capture type and source on the capture bar.
    kTypeSource,
    // In region mode, the UI to adjust a partial region.
    kSelection,
    // The button in the middle of a selection region to capture or record.
    kCaptureButton,
    // The buttons to open the settings menu and exit capture mode on the
    // capture bar.
    kSettingsClose,
    // A state where nothing is visibly focused. The next focus will advance to
    // the settings menu. This is to match tab focusing on other menus such as
    // quick settings.
    kPendingSettings,
    // The buttons inside the settings menu.
    kSettingsMenu,
  };

  // If a focusable capture session item is part of a views hierarchy, it needs
  // to override this class, which manages a custom focus ring.
  class HighlightableView {
   public:
    // Get the view class associated with |this|.
    virtual views::View* GetView() = 0;

    // Subclasses can override this for a custom focus ring shape. Defaults to
    // return nullptr, which means the focus ring will use the
    // HighlightPathGenerator used for clipping ink drops.
    virtual std::unique_ptr<views::HighlightPathGenerator>
    CreatePathGenerator();

    // Shows the focus ring and triggers setting accessibility focus on the
    // associated view.
    void PseudoFocus();

    // Hides the focus ring.
    void PseudoBlur();

    // Attempt to mimic a click on the associated view. Called by
    // CaptureModeSession when it receives a space key event, as the button is
    // not actuallly focused and will do nothing otherwise. Does nothing if the
    // view is not a subclass of Button.
    void ClickView();

   private:
    // TODO(crbug.com/1182456): This can result in multiple of these objects
    // thinking they have focus if CaptureModeSessionFocusCycler does not call
    // PseudoFocus or PseudoBlur properly. Investigate if there is a better
    // approach.
    bool has_focus_ = false;

    // A convenience pointer to the focus ring, which is owned by the views
    // hierarchy.
    views::FocusRing* focus_ring_ = nullptr;
  };

  explicit CaptureModeSessionFocusCycler(CaptureModeSession* session);
  CaptureModeSessionFocusCycler(const CaptureModeSessionFocusCycler&) = delete;
  CaptureModeSessionFocusCycler& operator=(
      const CaptureModeSessionFocusCycler&) = delete;
  ~CaptureModeSessionFocusCycler() override;

  // Advances the focus by simulating focus on a view, or updating the
  // CaptureModeSession to draw focus on elements which are not associated with
  // a view. The order is as follows:
  //   1) Capture mode type and source: (Screenshot/recording) and
  //      (fullscreen/region/window) on the capture bar.
  //   2) Region selection area: If visible.
  //   3) Capture/record button: If visible.
  //   4) Settings menu: If visible.
  //   5) Settings and close button: On the capture bar.
  // This should be called by CaptureModeSession when it receives a VKEY_TAB.
  void AdvanceFocus(bool reverse);

  // Removes focus. Called by CaptureModeSession when it receives a VKEY_ESC, or
  // when the state has changed such that a currently focus item is invalid
  // (i.e. switching from region mode to windowed mode makes a focused selection
  // or capture button invalid).
  void ClearFocus();

  // Returns true if anything has focus.
  bool HasFocus() const;

  // Called when the CaptureModeSession receives a VKEY_SPACE event. Returns
  // true if the focused view should take the event; when this happens the
  // CaptureModeSession should not handle the event.
  bool OnSpacePressed();

  // Returns true if the current focus group is associated with the UI used for
  // displaying a region.
  bool RegionGroupFocused() const;

  // Gets the current fine tune position for drawing the focus rects/rings on
  // the session's layer. Returns FineTunePosition::kNone if focus is on another
  // group.
  FineTunePosition GetFocusedFineTunePosition() const;

  // Called when the capture label widget is made visible or hidden, or changes
  // states. If the label button is visible, it should be on the a11y annotation
  // cycle, otherwise it should be removed from the a11y annotation cycle.
  void OnCaptureLabelWidgetUpdated();

  // Called when the settings menu is created. Starts observing the settings
  // menu.
  void OnSettingsMenuWidgetCreated();

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  friend class CaptureModeSessionTestApi;

  // Removes the focus ring from the current focused item if possible. Does not
  // alter |current_focus_group_| or |focus_index_|.
  void ClearCurrentVisibleFocus();

  // Get the next group in the focus order.
  FocusGroup GetNextGroup(bool reverse) const;

  // Returns the number of elements in a particular group.
  size_t GetGroupSize(FocusGroup group) const;

  // Returns the items of a particular |group|. Returns an empty array for the
  // kSelection group as the items in that group do not have associated views.
  std::vector<HighlightableView*> GetGroupItems(FocusGroup group) const;

  // Update the capture mode widgets so that accessibility features can traverse
  // between our widgets.
  void UpdateA11yAnnotation();

  views::Widget* GetSettingsMenuWidget() const;

  // The current focus group and focus index.
  FocusGroup current_focus_group_ = FocusGroup::kNone;
  size_t focus_index_ = 0u;

  // The session that owns |this|. Guaranteed to be non null for the lifetime of
  // |this|.
  CaptureModeSession* session_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      settings_menu_widget_observeration_{this};

  // True if the settings menu was opened via clicking space when the settings
  // button had focus. In this case, advancing focus will focus into the
  // settings menu. Otherwise, advancing focus with the settings menu open will
  // close the settings menu.
  bool settings_menu_opened_with_keyboard_nav_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SESSION_FOCUS_CYCLER_H_
