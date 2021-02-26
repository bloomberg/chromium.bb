// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_
#define ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace internal {

enum class IconType {
  kAudio,
  kArchive,
  kChart,
  kDrive,
  kExcel,
  kFolder,
  kFolderShared,
  kGdoc,
  kGdraw,
  kGeneric,
  kGform,
  kGmap,
  kGsheet,
  kGsite,
  kGslide,
  kGtable,
  kLinux,
  kImage,
  kPdf,
  kPpt,
  kScript,
  kSites,
  kTini,
  kVideo,
  kWord,
};

ASH_PUBLIC_EXPORT IconType
GetIconTypeFromString(const std::string& icon_type_string);

ASH_PUBLIC_EXPORT IconType GetIconTypeForPath(const base::FilePath& filepath);

ASH_PUBLIC_EXPORT gfx::ImageSkia GetVectorIconFromIconType(
    IconType icon,
    SkColor color,
    bool is_chip_icon = false);

ASH_PUBLIC_EXPORT int GetChipResourceIdForIconType(IconType icon);

}  // namespace internal

// Returns the file type icon for the specified `filepath`. Note that `color` is
// only used if the file type icon does not provide its own color.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetIconForPath(
    const base::FilePath& filepath,
    SkColor color = gfx::kGoogleGrey700);

// Returns the file type chip icon for the specified `filepath`. Note that
// `color` is only used if the file type chip icon does not provide its own
// color.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetChipIconForPath(
    const base::FilePath& filepath,
    SkColor color = gfx::kGoogleGrey700);

// Returns the file type icon for the specified `icon_type`. Note that `color`
// is only used if the file type icon does not provide its own color.
ASH_PUBLIC_EXPORT gfx::ImageSkia GetIconFromType(
    const std::string& icon_type,
    SkColor color = gfx::kGoogleGrey700);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FILE_ICON_UTIL_H_
