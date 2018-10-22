// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_

#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class NewTabPromoBubbleView;

///////////////////////////////////////////////////////////////////////////////
// NewTabButton
//
//  A subclass of ImageButton that hit-tests to the shape of the new tab button
//  and does custom drawing.
//
///////////////////////////////////////////////////////////////////////////////
class NewTabButton : public views::ImageButton,
                     public views::MaskedTargeterDelegate,
                     public views::WidgetObserver {
 public:
  NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener);
  ~NewTabButton() override;

  // Set the background X offset used to match the background image to the frame
  // image.
  void set_background_offset(int offset) {
    background_offset_ = offset;
  }

  // Retrieves the last active BrowserView instance to display the NewTabPromo.
  static void ShowPromoForLastActiveBrowser();

  // Returns whether there was a bubble that was closed. A bubble closes only
  // when it exists.
  static void CloseBubbleForLastActiveBrowser();

  // Shows the NewTabPromo when the NewTabFeatureEngagementTracker calls for it.
  void ShowPromo();

  // Returns whether there was a bubble that was closed. A bubble closes only
  // when it exists.
  void CloseBubble();

  // Called when the tab strip transitions to/from single tab mode, the frame
  // state changes or the accent color changes. Under Refresh this function will
  // update the glyph colors for the best contrast on the background.
  void FrameColorsChanged();

  void AnimateInkDropToStateForTesting(views::InkDropState state);

  NewTabPromoBubbleView* new_tab_promo() { return new_tab_promo_; }

 private:
// views::ImageButton:
#if defined(OS_WIN)
  void OnMouseReleased(const ui::MouseEvent& event) override;
#endif
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  void NotifyClick(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void Layout() override;
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(gfx::Path* mask) const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns whether this button should draw an incognito icon.
  bool ShouldDrawIncognitoIcon() const;

  // Returns the radius to use for the button corners (in newer material UI).
  int GetCornerRadius() const;

  // Computes a path corresponding to the button's outer border for a given
  // |scale| and stores it in |path|.  |button_y| is used as the y-coordinate
  // for the top of the button.  If |extend_to_top| is true, the path is
  // extended vertically to y = 0.  The caller uses this for Fitts' Law purposes
  // in maximized/fullscreen mode.
  void GetBorderPath(float button_y,
                     float scale,
                     bool extend_to_top,
                     SkPath* path) const;

  // Paints the fill region of the button into |canvas|, according to the
  // supplied values from GetImage() and the given |fill| path.
  void PaintFill(bool pressed,
                 float scale,
                 const SkPath& fill,
                 gfx::Canvas* canvas) const;

  // Paints a properly sized plus (+) icon into the center of the button.
  void PaintPlusIcon(gfx::Canvas* canvas, int offset, int size);

  SkColor GetButtonFillColor(bool opaque) const;
  SkColor GetIconColor() const;

  // In the touch-optimized UI, initializes the incognito button icon.
  void InitIncognitoIcon();

  // Returns the path for the newer material ui new tab button for the given
  // |scale|. |button_y| is the button's top y-cordinate. If |for_fill| is true,
  // the path will be shrunk by 1px from all sides to allow room for the stroke
  // to show up. If |extend_to_top| is true, the path is extended vertically to
  // y = 0.
  SkPath GetNewerMaterialUiButtonPath(float button_y,
                                      float scale,
                                      bool extend_to_top,
                                      bool for_fill) const;

  // Similar, but for the non-new material ui button.
  SkPath GetMaterialUiButtonPath(int button_y,
                                 int button_height,
                                 float scale,
                                 bool extend_to_top,
                                 bool for_fill) const;

  void UpdateInkDropBaseColor();

  // Tab strip that contains this button.
  TabStrip* tab_strip_;

  // Promotional UI that appears next to the NewTabButton and encourages its
  // use. Owned by its NativeWidget.
  NewTabPromoBubbleView* new_tab_promo_ = nullptr;

  // The offset used to paint the background image.
  int background_offset_;

  // Whether this new tab button belongs to a tabstrip that is part of an
  // incognito mode browser or not. Note that you can't drag a tab from one
  // incognito browser to another non-incognito browser or vice versa.
  const bool is_incognito_;

  // In the touch-optimized UI, the new tab button has a plus icon, and an
  // incognito icon if is in incognito mode.
  gfx::ImageSkia incognito_icon_;

  // In touch-optimized UI, this view holds the ink drop layer so that it's
  // shifted down to the correct top offset of the button, since the actual
  // button's y-coordinate is 0 due to Fitt's Law needs.
  views::InkDropContainerView* ink_drop_container_ = nullptr;

  // were we destroyed?
  bool* destroyed_ = nullptr;

  // Observes the NewTabPromo's Widget.  Used to tell whether the promo is
  // open and get called back when it closes.
  ScopedObserver<views::Widget, WidgetObserver> new_tab_promo_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(NewTabButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_NEW_TAB_BUTTON_H_
