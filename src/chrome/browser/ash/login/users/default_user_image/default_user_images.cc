// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace default_user_image {
namespace {

struct DefaultImageInfo {
  // Resource IDs of default user images.
  const int resource_id;

  // Message IDs of default user image descriptions.
  const int description_message_id;

  // Whether the user image is eligible in the current set. If so, user can
  // select the image as avatar through personalization settings.
  Eligibility eligibility;
};

// Info of default user images. When adding new entries to this list,
// please also update the enum ChromeOSUserImageId2 in
// tools/metrics/histograms/enums.xml
// When deprecating images, please also update kCurrentImageIndexes accordingly.
// clang-format off
constexpr DefaultImageInfo kDefaultImageInfo[] = {
    // No description for deprecated user image 0-18.
    {IDR_LOGIN_DEFAULT_USER, 0, Eligibility::kDeprecated},
    // Original set of images.
    {IDR_LOGIN_DEFAULT_USER_1, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_2, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_3, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_4, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_5, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_6, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_7, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_8, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_9, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_10, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_11, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_12, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_13, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_14, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_15, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_16, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_17, 0, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_18, 0, Eligibility::kDeprecated},
    // Second set of images.
    {IDR_LOGIN_DEFAULT_USER_19, IDS_LOGIN_DEFAULT_USER_DESC_19, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_20, IDS_LOGIN_DEFAULT_USER_DESC_20, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_21, IDS_LOGIN_DEFAULT_USER_DESC_21, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_22, IDS_LOGIN_DEFAULT_USER_DESC_22, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_23, IDS_LOGIN_DEFAULT_USER_DESC_23, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_24, IDS_LOGIN_DEFAULT_USER_DESC_24, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_25, IDS_LOGIN_DEFAULT_USER_DESC_25, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_26, IDS_LOGIN_DEFAULT_USER_DESC_26, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_27, IDS_LOGIN_DEFAULT_USER_DESC_27, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_28, IDS_LOGIN_DEFAULT_USER_DESC_28, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_29, IDS_LOGIN_DEFAULT_USER_DESC_29, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_30, IDS_LOGIN_DEFAULT_USER_DESC_30, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_31, IDS_LOGIN_DEFAULT_USER_DESC_31, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_32, IDS_LOGIN_DEFAULT_USER_DESC_32, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_33, IDS_LOGIN_DEFAULT_USER_DESC_33, Eligibility::kDeprecated},
    // Third set of images.
    {IDR_LOGIN_DEFAULT_USER_34, IDS_LOGIN_DEFAULT_USER_DESC_34, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_35, IDS_LOGIN_DEFAULT_USER_DESC_35, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_36, IDS_LOGIN_DEFAULT_USER_DESC_36, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_37, IDS_LOGIN_DEFAULT_USER_DESC_37, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_38, IDS_LOGIN_DEFAULT_USER_DESC_38, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_39, IDS_LOGIN_DEFAULT_USER_DESC_39, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_40, IDS_LOGIN_DEFAULT_USER_DESC_40, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_41, IDS_LOGIN_DEFAULT_USER_DESC_41, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_42, IDS_LOGIN_DEFAULT_USER_DESC_42, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_43, IDS_LOGIN_DEFAULT_USER_DESC_43, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_44, IDS_LOGIN_DEFAULT_USER_DESC_44, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_45, IDS_LOGIN_DEFAULT_USER_DESC_45, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_46, IDS_LOGIN_DEFAULT_USER_DESC_46, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_47, IDS_LOGIN_DEFAULT_USER_DESC_47, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_48, IDS_LOGIN_DEFAULT_USER_DESC_48, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_49, IDS_LOGIN_DEFAULT_USER_DESC_49, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_50, IDS_LOGIN_DEFAULT_USER_DESC_50, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_51, IDS_LOGIN_DEFAULT_USER_DESC_51, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_52, IDS_LOGIN_DEFAULT_USER_DESC_52, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_53, IDS_LOGIN_DEFAULT_USER_DESC_53, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_54, IDS_LOGIN_DEFAULT_USER_DESC_54, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_55, IDS_LOGIN_DEFAULT_USER_DESC_55, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_56, IDS_LOGIN_DEFAULT_USER_DESC_56, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_57, IDS_LOGIN_DEFAULT_USER_DESC_57, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_58, IDS_LOGIN_DEFAULT_USER_DESC_58, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_59, IDS_LOGIN_DEFAULT_USER_DESC_59, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_60, IDS_LOGIN_DEFAULT_USER_DESC_60, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_61, IDS_LOGIN_DEFAULT_USER_DESC_61, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_62, IDS_LOGIN_DEFAULT_USER_DESC_62, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_63, IDS_LOGIN_DEFAULT_USER_DESC_63, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_64, IDS_LOGIN_DEFAULT_USER_DESC_64, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_65, IDS_LOGIN_DEFAULT_USER_DESC_65, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_66, IDS_LOGIN_DEFAULT_USER_DESC_66, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_67, IDS_LOGIN_DEFAULT_USER_DESC_67, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_68, IDS_LOGIN_DEFAULT_USER_DESC_68, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_69, IDS_LOGIN_DEFAULT_USER_DESC_69, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_70, IDS_LOGIN_DEFAULT_USER_DESC_70, Eligibility::kDeprecated},
    {IDR_LOGIN_DEFAULT_USER_71, IDS_LOGIN_DEFAULT_USER_DESC_71, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_72, IDS_LOGIN_DEFAULT_USER_DESC_72, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_73, IDS_LOGIN_DEFAULT_USER_DESC_73, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_74, IDS_LOGIN_DEFAULT_USER_DESC_74, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_75, IDS_LOGIN_DEFAULT_USER_DESC_75, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_76, IDS_LOGIN_DEFAULT_USER_DESC_76, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_77, IDS_LOGIN_DEFAULT_USER_DESC_77, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_78, IDS_LOGIN_DEFAULT_USER_DESC_78, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_79, IDS_LOGIN_DEFAULT_USER_DESC_79, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_80, IDS_LOGIN_DEFAULT_USER_DESC_80, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_81, IDS_LOGIN_DEFAULT_USER_DESC_81, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_82, IDS_LOGIN_DEFAULT_USER_DESC_82, Eligibility::kEligible},
    // Material design avatars.
    {IDR_LOGIN_DEFAULT_USER_83, IDS_LOGIN_DEFAULT_USER_DESC_83, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_84, IDS_LOGIN_DEFAULT_USER_DESC_84, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_85, IDS_LOGIN_DEFAULT_USER_DESC_85, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_86, IDS_LOGIN_DEFAULT_USER_DESC_86, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_87, IDS_LOGIN_DEFAULT_USER_DESC_87, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_88, IDS_LOGIN_DEFAULT_USER_DESC_88, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_89, IDS_LOGIN_DEFAULT_USER_DESC_89, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_90, IDS_LOGIN_DEFAULT_USER_DESC_90, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_91, IDS_LOGIN_DEFAULT_USER_DESC_91, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_92, IDS_LOGIN_DEFAULT_USER_DESC_92, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_93, IDS_LOGIN_DEFAULT_USER_DESC_93, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_94, IDS_LOGIN_DEFAULT_USER_DESC_94, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_95, IDS_LOGIN_DEFAULT_USER_DESC_95, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_96, IDS_LOGIN_DEFAULT_USER_DESC_96, Eligibility::kEligible},
    {IDR_LOGIN_DEFAULT_USER_97, IDS_LOGIN_DEFAULT_USER_DESC_97, Eligibility::kEligible},
};
// clang-format on

// Indexes of the current set of default images in the order that will display
// in the personalization settings page. This list should contain all the
// indexes of egligible default images listed above.
// clang-format off
constexpr int kCurrentImageIndexes[] = {
    // Material design avatars.
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    // Third set of images.
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
};
// clang-format on

// Compile time check that make sure the current default images are the set of
// all the eligible default images.
constexpr bool ValidateCurrentImageIndexes() {
  int num_eligible_images = 0;
  for (const auto info : kDefaultImageInfo) {
    if (info.eligibility == Eligibility::kEligible)
      num_eligible_images++;
  }
  if (num_eligible_images != base::size(kCurrentImageIndexes))
    return false;

  for (const int index : kCurrentImageIndexes) {
    if (kDefaultImageInfo[index].eligibility != Eligibility::kEligible)
      return false;
  }
  return true;
}

static_assert(ValidateCurrentImageIndexes(),
              "kCurrentImageIndexes should contain all the indexes of "
              "egligible default images listed in kDefaultImageInfo.");

// Source info of (deprecated) default user images.
const DefaultImageSourceInfo kDefaultImageSourceInfo[] = {
    {IDS_LOGIN_DEFAULT_USER_AUTHOR, IDS_LOGIN_DEFAULT_USER_WEBSITE},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_1, IDS_LOGIN_DEFAULT_USER_WEBSITE_1},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_2, IDS_LOGIN_DEFAULT_USER_WEBSITE_2},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_3, IDS_LOGIN_DEFAULT_USER_WEBSITE_3},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_4, IDS_LOGIN_DEFAULT_USER_WEBSITE_4},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_5, IDS_LOGIN_DEFAULT_USER_WEBSITE_5},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_6, IDS_LOGIN_DEFAULT_USER_WEBSITE_6},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_7, IDS_LOGIN_DEFAULT_USER_WEBSITE_7},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_8, IDS_LOGIN_DEFAULT_USER_WEBSITE_8},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_9, IDS_LOGIN_DEFAULT_USER_WEBSITE_9},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_10, IDS_LOGIN_DEFAULT_USER_WEBSITE_10},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_11, IDS_LOGIN_DEFAULT_USER_WEBSITE_11},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_12, IDS_LOGIN_DEFAULT_USER_WEBSITE_12},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_13, IDS_LOGIN_DEFAULT_USER_WEBSITE_13},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_14, IDS_LOGIN_DEFAULT_USER_WEBSITE_14},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_15, IDS_LOGIN_DEFAULT_USER_WEBSITE_15},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_16, IDS_LOGIN_DEFAULT_USER_WEBSITE_16},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_17, IDS_LOGIN_DEFAULT_USER_WEBSITE_17},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_18, IDS_LOGIN_DEFAULT_USER_WEBSITE_18},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_19, IDS_LOGIN_DEFAULT_USER_WEBSITE_19},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_20, IDS_LOGIN_DEFAULT_USER_WEBSITE_20},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_21, IDS_LOGIN_DEFAULT_USER_WEBSITE_21},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_22, IDS_LOGIN_DEFAULT_USER_WEBSITE_22},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_23, IDS_LOGIN_DEFAULT_USER_WEBSITE_23},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_24, IDS_LOGIN_DEFAULT_USER_WEBSITE_24},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_25, IDS_LOGIN_DEFAULT_USER_WEBSITE_25},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_26, IDS_LOGIN_DEFAULT_USER_WEBSITE_26},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_27, IDS_LOGIN_DEFAULT_USER_WEBSITE_27},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_28, IDS_LOGIN_DEFAULT_USER_WEBSITE_28},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_29, IDS_LOGIN_DEFAULT_USER_WEBSITE_29},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_30, IDS_LOGIN_DEFAULT_USER_WEBSITE_30},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_31, IDS_LOGIN_DEFAULT_USER_WEBSITE_31},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_32, IDS_LOGIN_DEFAULT_USER_WEBSITE_32},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_33, IDS_LOGIN_DEFAULT_USER_WEBSITE_33},
};

const char kDefaultUrlPrefix[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER_";
const char kZeroDefaultUrl[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER";

// Returns true if the string specified consists of the prefix and one of
// the default images indices. Returns the index of the image in `image_id`
// variable.
bool IsDefaultImageString(const std::string& s,
                          const std::string& prefix,
                          int* image_id) {
  DCHECK(image_id);
  if (!base::StartsWith(s, prefix, base::CompareCase::SENSITIVE))
    return false;

  int image_index = -1;
  if (base::StringToInt(
          base::MakeStringPiece(s.begin() + prefix.length(), s.end()),
          &image_index)) {
    if (image_index < 0 || image_index >= kDefaultImagesCount)
      return false;
    *image_id = image_index;
    return true;
  }

  return false;
}

}  // namespace

const int kDefaultImagesCount = base::size(kDefaultImageInfo);

const int kFirstDefaultImageIndex = 48;

// Limit random default image index to prevent undesirable UI behavior when
// selecting an image with a high index. E.g. automatic scrolling of picture
// list that is used to present default images.
const int kLastRandomDefaultImageIndex = 62;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The order and the values of these constants are important for histograms
// of different Chrome OS versions to be merged smoothly.
const int kHistogramImageFromCamera = 0;
const int kHistogramImageExternal = 1;
const int kHistogramImageFromProfile = 2;
// The special images max count is used to reserve a histogram range (0-9) for
// special images. Default images will have their histogram value starting
// at 10. Check ChromeOSUserImageId in tools/metrics/histograms/enums.xml to see
// how these values are mapped.
const int kHistogramSpecialImagesMaxCount = 10;
const int kHistogramImagesCount =
    kDefaultImagesCount + kHistogramSpecialImagesMaxCount;

std::string GetDefaultImageUrl(int index) {
  if (index <= 0 || index >= kDefaultImagesCount)
    return kZeroDefaultUrl;
  return base::StringPrintf("%s%d", kDefaultUrlPrefix, index);
}

bool IsDefaultImageUrl(const std::string& url, int* image_id) {
  if (url == kZeroDefaultUrl) {
    *image_id = 0;
    return true;
  }
  return IsDefaultImageString(url, kDefaultUrlPrefix, image_id);
}

const gfx::ImageSkia& GetDefaultImage(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kDefaultImageInfo[index].resource_id);
}

const int GetDefaultImageResourceId(int index) {
  return kDefaultImageInfo[index].resource_id;
}

int GetRandomDefaultImageIndex() {
  return base::RandInt(kFirstDefaultImageIndex, kLastRandomDefaultImageIndex);
}

bool IsValidIndex(int index) {
  return index >= 0 && index < kDefaultImagesCount;
}

bool IsInCurrentImageSet(int index) {
  return IsValidIndex(index) &&
         kDefaultImageInfo[index].eligibility == Eligibility::kEligible;
}

std::unique_ptr<base::ListValue> GetCurrentImageSet() {
  auto image_urls = std::make_unique<base::ListValue>();
  for (int i = 0; i < base::size(kCurrentImageIndexes); ++i) {
    auto image_data = std::make_unique<base::DictionaryValue>();
    int index = kCurrentImageIndexes[i];
    int string_id = kDefaultImageInfo[index].description_message_id;

    image_data->SetString("url", default_user_image::GetDefaultImageUrl(index));
    image_data->SetInteger("index", index);
    image_data->SetString("title", string_id
                                       ? l10n_util::GetStringUTF16(string_id)
                                       : std::u16string());
    image_urls->Append(std::move(image_data));
  }
  return image_urls;
}

absl::optional<DefaultImageSourceInfo> GetDefaultImageSourceInfo(int index) {
  if (index >= base::size(kDefaultImageSourceInfo))
    return absl::nullopt;

  return kDefaultImageSourceInfo[index];
}

}  // namespace default_user_image
}  // namespace ash
