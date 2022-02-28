// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/scoped_observation.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/views/chrome_view_class_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/views/lens/lens_side_panel_controller.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

SidePanelToolbarButton::SidePanelToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&SidePanelToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      dot_indicator_(views::DotIndicator::Install(image())),
      dot_bounds_updater_(
          std::make_unique<DotBoundsUpdater>(dot_indicator_, image())),
      reading_list_model_(
          ReadingListModelFactory::GetForBrowserContext(browser_->profile())) {
  SetVectorIcons(kSidePanelIcon, kSidePanelTouchIcon);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
  SetProperty(views::kElementIdentifierKey, kReadLaterButtonElementId);

  if (reading_list_model_)
    reading_list_model_scoped_observation_.Observe(reading_list_model_.get());
}

SidePanelToolbarButton::~SidePanelToolbarButton() = default;

SidePanelToolbarButton::DotBoundsUpdater::DotBoundsUpdater(
    views::DotIndicator* dot_indicator,
    views::ImageView* image)
    : dot_indicator_(dot_indicator), image_(image) {
  observation_.Observe(image);
}

SidePanelToolbarButton::DotBoundsUpdater::~DotBoundsUpdater() = default;

void SidePanelToolbarButton::DotBoundsUpdater::OnViewBoundsChanged(
    View* observed_view) {
  gfx::Rect dot_rect(8, 8);
  if (ui::TouchUiController::Get()->touch_ui()) {
    dot_rect = ScaleToEnclosingRect(
        dot_rect, float{kDefaultTouchableIconSize} / kDefaultIconSize);
  }
  dot_rect.set_origin(image_->GetImageBounds().bottom_right() -
                      dot_rect.bottom_right().OffsetFromOrigin());
  dot_indicator_->SetBoundsRect(dot_rect);
}

void SidePanelToolbarButton::ReadingListModelLoaded(
    const ReadingListModel* model) {
  if (model->unseen_size())
    dot_indicator_->Show();
}

void SidePanelToolbarButton::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  DCHECK(reading_list_model_scoped_observation_.IsObservingSource(
      reading_list_model_.get()));
  reading_list_model_scoped_observation_.Reset();
}

void SidePanelToolbarButton::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  if (!side_panel_webview_ && reading_list_model_->unseen_size() > 0) {
    dot_indicator_->Show();
  } else {
    dot_indicator_->Hide();
  }
}

void SidePanelToolbarButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->right_aligned_side_panel());

  if (browser_view->side_panel_coordinator()) {
    browser_view->side_panel_coordinator()->Toggle();
    return;
  }

  if (!side_panel_webview_) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Hide the Lens side panel if it's showing instead.
    lens::LensSidePanelController* const lens_side_panel_controller =
        browser_view->lens_side_panel_controller();
    if (lens_side_panel_controller && lens_side_panel_controller->IsShowing()) {
      lens_side_panel_controller->Close();
      return;
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    // Using base::Unretained(this) is safe here because the side panel (and the
    // web view as its child) will be destroyed before the toolbar which will
    // destroy the SidePanelToolbarButton.
    auto webview = std::make_unique<ReadLaterSidePanelWebView>(
        browser_, base::BindRepeating(&SidePanelToolbarButton::ButtonPressed,
                                      base::Unretained(this)));
    side_panel_webview_ =
        browser_view->right_aligned_side_panel()->AddChildView(
            std::move(webview));
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));
    reading_list_model_->MarkAllSeen();
    dot_indicator_->Hide();
  } else {
    HideSidePanel();
  }
}

void SidePanelToolbarButton::HideSidePanel() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->right_aligned_side_panel());
  if (side_panel_webview_) {
    browser_view->right_aligned_side_panel()->RemoveChildViewT(
        side_panel_webview_.get());
    side_panel_webview_ = nullptr;
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));
  }
}

bool SidePanelToolbarButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}
