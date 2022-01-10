// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class RoundedImageView;

// A class for loading and displaying the icon of apps/urls used in a
// DesksTemplatesItemView. Depending on the `count_` and `icon_identifier_`,
// this View may have only an icon, only a count label, or both.
class DesksTemplatesIconView : public views::View {
 public:
  METADATA_HEADER(DesksTemplatesIconView);

  DesksTemplatesIconView();
  DesksTemplatesIconView(const DesksTemplatesIconView&) = delete;
  DesksTemplatesIconView& operator=(const DesksTemplatesIconView&) = delete;
  ~DesksTemplatesIconView() override;

  // The size of an icon.
  static constexpr int kIconSize = 28;

  const std::string& icon_identifier() const { return icon_identifier_; }

  int count() const { return count_; }

  // Sets `icon_identifier_` to `icon_identifier` and `count_` to `count` then
  // based on their values determines what views need to be created and starts
  // loading the icon specified by `icon_identifier`.
  void SetIconIdentifierAndCount(const std::string& icon_identifier, int count);

  // Sets `count_` to `count` and updates the `count_label_`.
  void UpdateCount(int count);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

 private:
  friend class DesksTemplatesIconViewTestApi;

  // Callbacks for when the app icon/favicon has been fetched. If the result is
  // non-null/empty then we'll set this's image to the result. Otherwise, we'll
  // use a placeholder icon.
  void OnFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& image_result);
  void OnAppIconLoaded(apps::IconValuePtr icon_value);

  // Loads the default favicon to `icon_view_`. Should be called when we fail to
  // load an icon.
  void LoadDefaultIcon();

  // The identifier for an icon. For a favicon, this will be a url. For an app,
  // this will be an app id.
  std::string icon_identifier_;

  // The number of instances of this icon's respective app/url stored in this's
  // respective DeskTemplate.
  int count_;

  // Owned by the views hierarchy.
  views::Label* count_label_ = nullptr;
  RoundedImageView* icon_view_ = nullptr;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;

  base::WeakPtrFactory<DesksTemplatesIconView> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTemplatesIconView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesIconView)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_VIEW_H_
