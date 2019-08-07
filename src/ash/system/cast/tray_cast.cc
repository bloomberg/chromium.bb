// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/interfaces/cast_config.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

namespace {

// Returns the correct vector icon for |icon_type|. Some types may be different
// for branded builds.
const gfx::VectorIcon& SinkIconTypeToIcon(mojom::SinkIconType icon_type) {
  switch (icon_type) {
#if defined(GOOGLE_CHROME_BUILD)
    case mojom::SinkIconType::CAST:
      return kSystemMenuCastDeviceIcon;
    case mojom::SinkIconType::EDUCATION:
      return kSystemMenuCastEducationIcon;
    case mojom::SinkIconType::HANGOUT:
      return kSystemMenuCastHangoutIcon;
    case mojom::SinkIconType::MEETING:
      return kSystemMenuCastMeetingIcon;
#else
    case mojom::SinkIconType::CAST:
    case mojom::SinkIconType::EDUCATION:
      return kSystemMenuCastGenericIcon;
    case mojom::SinkIconType::HANGOUT:
    case mojom::SinkIconType::MEETING:
      return kSystemMenuCastMessageIcon;
#endif
    case mojom::SinkIconType::GENERIC:
      return kSystemMenuCastGenericIcon;
    case mojom::SinkIconType::CAST_AUDIO_GROUP:
      return kSystemMenuCastAudioGroupIcon;
    case mojom::SinkIconType::CAST_AUDIO:
      return kSystemMenuCastAudioIcon;
    case mojom::SinkIconType::WIRED_DISPLAY:
      return kSystemMenuCastGenericIcon;
  }

  NOTREACHED();
  return kSystemMenuCastGenericIcon;
}

}  // namespace

namespace tray {

CastDetailedView::CastDetailedView(
    DetailedViewDelegate* delegate,
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes)
    : TrayDetailedView(delegate) {
  CreateItems();
  UpdateReceiverList(sinks_routes);
}

CastDetailedView::~CastDetailedView() = default;

void CastDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_CAST);
}

void CastDetailedView::UpdateReceiverList(
    const std::vector<mojom::SinkAndRoutePtr>& sinks_routes) {
  // Add/update existing.
  for (const auto& it : sinks_routes)
    sinks_and_routes_[it->sink->id] = it->Clone();

  // Remove non-existent sinks. Removing an element invalidates all existing
  // iterators.
  auto i = sinks_and_routes_.begin();
  while (i != sinks_and_routes_.end()) {
    bool has_receiver = false;
    for (auto& receiver : sinks_routes) {
      if (i->first == receiver->sink->id)
        has_receiver = true;
    }

    if (has_receiver)
      ++i;
    else
      i = sinks_and_routes_.erase(i);
  }

  // Update UI.
  UpdateReceiverListFromCachedData();
  Layout();
}

void CastDetailedView::UpdateReceiverListFromCachedData() {
  // Remove all of the existing views.
  view_to_sink_map_.clear();
  scroll_content()->RemoveAllChildViews(true);

  // Add a view for each receiver.
  for (auto& it : sinks_and_routes_) {
    const ash::mojom::SinkAndRoutePtr& sink_route = it.second;
    const base::string16 name = base::UTF8ToUTF16(sink_route->sink->name);
    views::View* container = AddScrollListItem(
        SinkIconTypeToIcon(sink_route->sink->sink_icon_type), name);
    view_to_sink_map_[container] = sink_route->sink.Clone();
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

void CastDetailedView::HandleViewClicked(views::View* view) {
  // Find the receiver we are going to cast to.
  auto it = view_to_sink_map_.find(view);
  if (it != view_to_sink_map_.end()) {
    Shell::Get()->cast_config()->CastToSink(it->second.Clone());
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST);
  }
}

}  // namespace tray
}  // namespace ash
