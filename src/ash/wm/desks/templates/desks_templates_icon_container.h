// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_

#include <string>
#include <utility>
#include <vector>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;
class DesksTemplatesIconView;

// This class for determines which app icons/favicons to show for a desk
// template and creates the according DesksTemplatesIconView's for them.
// The last DesksTemplatesIconView in the layout is used for storing the
// overflow count of icons. Not every view in the container is visible.
//   _______________________________________________________________________
//   |  _________  _________   _________________   _________   _________   |
//   |  |       |  |       |   |       |       |   |       |   |       |   |
//   |  |   I   |  |   I   |   |   I      + N  |   |   I   |   |  + N  |   |
//   |  |_______|  |_______|   |_______|_______|   |_______|   |_______|   |
//   |_____________________________________________________________________|
//
// If there are multiple apps associated with a certain icon, the icon is drawn
// once with a +N label attached, up to +99. If there are too many icons to be
// displayed within the given width, we draw as many and a label at the end that
// says +N, up to +99.
class DesksTemplatesIconContainer : public views::BoxLayoutView {
 public:
  METADATA_HEADER(DesksTemplatesIconContainer);

  DesksTemplatesIconContainer();
  DesksTemplatesIconContainer(const DesksTemplatesIconContainer&) = delete;
  DesksTemplatesIconContainer& operator=(const DesksTemplatesIconContainer&) =
      delete;
  ~DesksTemplatesIconContainer() override;

  // The maximum number of icons that can be displayed.
  static constexpr int kMaxIcons = 4;

  const ui::ColorProvider* incognito_window_color_provider() const {
    return incognito_window_color_provider_;
  }

  // Given a desk template, determine which icons to show in this and create
  // the according DesksTemplatesIconView's.
  void PopulateIconContainerFromTemplate(DeskTemplate* desk_template);

  // Given `windows`, determine which icons to show in this and create the
  // according DesksTemplatesIconView's.
  void PopulateIconContainerFromWindows(
      const std::vector<aura::Window*>& windows);

  // views::BoxLayoutView:
  void Layout() override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  // Given an ordered vector of pairs, where the first entry is an icon's
  // identifier and the second entry is its count, create views for them.
  void CreateIconViewsFromIconIdentifiers(
      const std::vector<std::pair<std::string, int>>& identifiers_and_counts);

  // If `this` is created with an incognito window, store the ui::ColorProvider
  // of one of the incognito windows to retrieve its icon's color.
  const ui::ColorProvider* incognito_window_color_provider_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesIconContainer,
                   views::BoxLayoutView)
VIEW_BUILDER_METHOD(PopulateIconContainerFromTemplate, DeskTemplate*)
VIEW_BUILDER_METHOD(PopulateIconContainerFromWindows,
                    const std::vector<aura::Window*>&)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesIconContainer)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_
