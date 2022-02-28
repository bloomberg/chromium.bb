// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/reorder/app_list_reorder_util.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/ash/app_icon_color_cache.h"
#include "ui/gfx/color_analysis.h"

namespace app_list {
namespace reorder {

const float kOrderResetThreshold = 0.2f;

// On the 360 degree hue color spectrum, this value is used as a cutuff to
// indicate that any value equal to or higher than this is considered red.
const float kRedHueCutoff = 315.0f;

// An hsv color with saturation below this cutoff will be categorized as either
// black or white.
const float kBlackWhiteSaturationCutoff = 0.1f;

// When an hsv color has a saturation below 'kBlackWhiteSaturationCutoff' then
// if its value is below this cutoff it will be categorized as white and with a
// value above this cutoff is will be categorized as black.
const float kBlackWhiteLowSaturatonValueCutoff = 0.9f;

// An hsv color with a value less than this cutoff will be categorized as black.
const float kBlackValueCutoff = 0.35f;

// ReorderParam ----------------------------------------------------------------

ReorderParam::ReorderParam(const std::string& new_sync_item_id,
                           const syncer::StringOrdinal& new_ordinal)
    : sync_item_id(new_sync_item_id), ordinal(new_ordinal) {}

ReorderParam::ReorderParam(const ReorderParam&) = default;

ReorderParam::~ReorderParam() = default;

// SyncItemWrapper<std::string> ------------------------------------------------

template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item)
    : id(sync_item.item_id),
      item_ordinal(sync_item.item_ordinal),
      is_folder(sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER),
      key_attribute(sync_item.item_name) {}

template <>
SyncItemWrapper<std::string>::SyncItemWrapper(
    const ChromeAppListItem& app_list_item)
    : id(app_list_item.id()),
      item_ordinal(app_list_item.position()),
      is_folder(app_list_item.is_folder()),
      key_attribute(app_list_item.name()) {}

// SyncItemWrapper<ash::IconColor> ---------------------------------------------

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item)
    : id(sync_item.item_id),
      item_ordinal(sync_item.item_ordinal),
      is_folder(sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER),
      key_attribute(sync_item.item_color) {}

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const ChromeAppListItem& app_list_item)
    : id(app_list_item.id()),
      item_ordinal(app_list_item.position()),
      is_folder(app_list_item.is_folder()),
      key_attribute(app_list_item.icon_color()) {}

// Color Sort Utilities -------------------------------------------------------

sync_pb::AppListSpecifics::ColorGroup CalculateBackgroundColorGroup(
    const SkBitmap& source,
    sync_pb::AppListSpecifics::ColorGroup light_vibrant_group) {
  if (source.empty()) {
    return sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_WHITE;
  }

  DCHECK_EQ(kN32_SkColorType, source.info().colorType());

  int width = source.width();
  int height = source.height();

  sync_pb::AppListSpecifics::ColorGroup left_group = sync_pb::AppListSpecifics::
      ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;
  sync_pb::AppListSpecifics::ColorGroup right_group = sync_pb::
      AppListSpecifics::ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;

  // Find the color group for the first opaque pixel on the left edge of the
  // icon.
  const SkColor* current =
      reinterpret_cast<SkColor*>(source.getAddr32(0, height / 2));
  for (int x = 0; x < width; ++x, ++current) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    } else {
      left_group = ColorToColorGroup(*current);
      break;
    }
  }

  // Find the color group for the first opaque pixel on the right edge of the
  // icon.
  current = reinterpret_cast<SkColor*>(source.getAddr32(width - 1, height / 2));
  for (int x = width - 1; x >= 0; --x, --current) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    } else {
      right_group = ColorToColorGroup(*current);
      break;
    }
  }

  // If the left and right edge have the same color grouping, then return that
  // group as the calculated background color group.
  if (left_group == right_group)
    return left_group;

  // Find the color group for the first opaque pixel on the top edge of the
  // icon.
  sync_pb::AppListSpecifics::ColorGroup top_group = sync_pb::AppListSpecifics::
      ColorGroup::AppListSpecifics_ColorGroup_COLOR_BLACK;
  current = reinterpret_cast<SkColor*>(source.getAddr32(width / 2, 0));
  const int row_bytes = source.rowBytes();
  for (int y = 0; y < height; ++y, current += row_bytes) {
    if (SkColorGetA(*current) < SK_AlphaOPAQUE) {
      continue;
    } else {
      top_group = ColorToColorGroup(*current);
      break;
    }
  }

  // If the top edge has a matching color group with the left or right group,
  // then return that group.
  if (top_group == right_group || top_group == left_group)
    return top_group;

  // When all three sampled color groups are different, then there is no
  // conclusive color group for the icon's background. Return the group
  // corresponding to the app icon's light vibrant color.
  return light_vibrant_group;
}

sync_pb::AppListSpecifics::ColorGroup ColorToColorGroup(SkColor color) {
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);

  const float h = hsv[0];
  const float s = hsv[1];
  const float v = hsv[2];

  sync_pb::AppListSpecifics::ColorGroup group;

  // Assign the ColorGroup based on the hue of `color`. Each if statement checks
  // the value of the hue and groups the color based on it. These values are
  // approximations for grouping like colors together.
  if (h < 15) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_RED;
  } else if (h < 45) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_ORANGE;
  } else if (h < 75) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_YELLOW;
  } else if (h < 182) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_GREEN;
  } else if (h < 255) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_BLUE;
  } else if (h < kRedHueCutoff) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_MAGENTA;
  } else {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_RED;
  }

  if (s < kBlackWhiteSaturationCutoff) {
    if (v < kBlackWhiteLowSaturatonValueCutoff) {
      group = sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_BLACK;
    } else {
      group = sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_WHITE;
    }
  } else if (v < kBlackValueCutoff) {
    group = sync_pb::AppListSpecifics::ColorGroup::
        AppListSpecifics_ColorGroup_COLOR_BLACK;
  }

  return group;
}

ash::IconColor GetSortableIconColorForApp(const std::string& id,
                                          const gfx::ImageSkia& image) {
  SkColor extracted_light_vibrant_color =
      ash::AppIconColorCache::GetInstance().GetLightVibrantColorForApp(id,
                                                                       image);

  sync_pb::AppListSpecifics::ColorGroup light_vibrant_color_group =
      ColorToColorGroup(extracted_light_vibrant_color);

  ash::IconColor sortable_icon_color;
  sync_pb::AppListSpecifics::ColorGroup background_color_group =
      CalculateBackgroundColorGroup(*image.bitmap(), light_vibrant_color_group);

  // `hue` represents the hue of the extracted light vibrant color and can be
  // defined by the interval [-1, 360], where -1 (kHueMin) denotes that the hue
  // should come before all other hue values, and 360 (kHueMax) denotes that the
  // hue should come after all other hue values.
  int hue;

  if (light_vibrant_color_group ==
      sync_pb::AppListSpecifics::ColorGroup::
          AppListSpecifics_ColorGroup_COLOR_BLACK) {
    // If `light_vibrant_color_group` is black it should be ordered after all
    // other hues.
    hue = ash::IconColor::kHueMax;
  } else if (light_vibrant_color_group ==
             sync_pb::AppListSpecifics::ColorGroup::
                 AppListSpecifics_ColorGroup_COLOR_WHITE) {
    // If 'light_vibrant_color_group' is white, then the hue should be ordered
    // before all other hues.
    hue = ash::IconColor::kHueMin;

  } else {
    SkScalar hsv[3];
    SkColorToHSV(extracted_light_vibrant_color, hsv);
    hue = hsv[0];

    // If the hue is a red on the high end of the hsv color spectrum, then
    // subtract the maximum possible hue so that reds on the high end of the hsv
    // color spectrum are ordered next to reds on the low end of the hsv color
    // spectrum.
    if (hue >= kRedHueCutoff)
      hue -= ash::IconColor::kHueMax;

    // Shift up the hue value so that the returned hue value always remains
    // within the interval [0, 360].
    hue += (ash::IconColor::kHueMax - kRedHueCutoff);

    DCHECK_GE(hue, 0);
    DCHECK_LE(hue, ash::IconColor::kHueMax);
  }

  return ash::IconColor(background_color_group, hue);
}

}  // namespace reorder
}  // namespace app_list
