// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace send_tab_to_self {

namespace {

std::unique_ptr<views::ImageView> CreateIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  static constexpr int kPrimaryIconSize = 20;
  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      device_type == sync_pb::SyncEnums::TYPE_PHONE ? kHardwareSmartphoneIcon
                                                    : kHardwareComputerIcon,
      ui::kColorIcon, kPrimaryIconSize));
  constexpr auto kPrimaryIconBorder = gfx::Insets(6);
  icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon;
}

std::u16string GetLastUpdatedTime(const TargetDeviceInfo& device_info) {
  int time_in_days =
      (base::Time::Now() - device_info.last_updated_timestamp).InDays();
  if (time_in_days == 0) {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_TODAY_SEND_TAB_TO_SELF);
  } else if (time_in_days == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAY_SEND_TAB_TO_SELF,
        base::UTF8ToUTF16(std::to_string(time_in_days)));
  }
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAYS_SEND_TAB_TO_SELF,
      base::UTF8ToUTF16(std::to_string(time_in_days)));
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    SendTabToSelfBubbleViewImpl* bubble,
    const TargetDeviceInfo& device_info)
    : HoverButton(
          base::BindRepeating(&SendTabToSelfBubbleViewImpl::DeviceButtonPressed,
                              base::Unretained(bubble),
                              base::Unretained(this)),
          CreateIcon(device_info.device_type),
          base::UTF8ToUTF16(device_info.device_name),
          GetLastUpdatedTime(device_info)) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  device_type_ = device_info.device_type;
  SetEnabled(true);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

BEGIN_METADATA(SendTabToSelfBubbleDeviceButton, HoverButton)
END_METADATA

}  // namespace send_tab_to_self
