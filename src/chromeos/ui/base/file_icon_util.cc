// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/file_icon_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace chromeos {
namespace {

constexpr int kIconDefaultDipSize = 20;

enum class ColorId {
  kBlue,
  kGreen,
  kGrey,
  kRed,
  kYellow,
  kFiletypeGsite,
  kFiletypePpt,
  kFiletypeSites,
};

struct IconParams {
  const gfx::VectorIcon& icon;
  ColorId color_id;
};

SkColor ResolveColor(ColorId color_id, bool dark_background) {
  switch (color_id) {
    case ColorId::kBlue:
      return dark_background ? gfx::kGoogleBlue300 : gfx::kGoogleBlue500;
    case ColorId::kGreen:
      return dark_background ? gfx::kGoogleGreen300 : gfx::kGoogleGreen500;
    case ColorId::kGrey:
      return dark_background ? gfx::kGoogleGrey200 : gfx::kGoogleGrey700;
    case ColorId::kRed:
      return dark_background ? gfx::kGoogleRed300 : gfx::kGoogleRed500;
    case ColorId::kYellow:
      return dark_background ? gfx::kGoogleYellow300 : gfx::kGoogleYellow500;
    case ColorId::kFiletypeGsite:
      return SkColorSetRGB(0x79, 0x6E, 0xEF);
    case ColorId::kFiletypePpt:
      return SkColorSetRGB(0xFF, 0x76, 0x37);
    case ColorId::kFiletypeSites:
      return dark_background ? SkColorSetRGB(0xBC, 0xB7, 0xF7)
                             : SkColorSetRGB(0x79, 0x6E, 0xEE);
  }
}

const std::map<IconType, IconParams>& GetIconTypeToIconParamsMap() {
  // Changes to this map should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.
  static const base::NoDestructor<std::map<IconType, IconParams>>
      icon_type_to_icon_params(
          {{IconType::kArchive,
            IconParams{kFiletypeArchiveIcon, ColorId::kGrey}},
           {IconType::kAudio, IconParams{kFiletypeAudioIcon, ColorId::kRed}},
           {IconType::kChart, IconParams{kFiletypeChartIcon, ColorId::kGreen}},
           {IconType::kDrive,
            IconParams{kFiletypeTeamDriveIcon, ColorId::kGrey}},
           {IconType::kExcel, IconParams{kFiletypeExcelIcon, ColorId::kGreen}},
           {IconType::kFolder, IconParams{kFiletypeFolderIcon, ColorId::kGrey}},
           {IconType::kFolderShared,
            IconParams{kFiletypeSharedIcon, ColorId::kGrey}},
           {IconType::kGdoc, IconParams{kFiletypeGdocIcon, ColorId::kBlue}},
           {IconType::kGdraw, IconParams{kFiletypeGdrawIcon, ColorId::kRed}},
           {IconType::kGeneric,
            IconParams{kFiletypeGenericIcon, ColorId::kGrey}},
           {IconType::kGform, IconParams{kFiletypeGformIcon, ColorId::kGreen}},
           {IconType::kGmap, IconParams{kFiletypeGmapIcon, ColorId::kRed}},
           {IconType::kGsheet,
            IconParams{kFiletypeGsheetIcon, ColorId::kGreen}},
           {IconType::kGsite,
            IconParams{kFiletypeGsiteIcon, ColorId::kFiletypeGsite}},
           {IconType::kGslide,
            IconParams{kFiletypeGslidesIcon, ColorId::kYellow}},
           {IconType::kGtable,
            IconParams{kFiletypeGtableIcon, ColorId::kGreen}},
           {IconType::kImage, IconParams{kFiletypeImageIcon, ColorId::kRed}},
           {IconType::kLinux, IconParams{kFiletypeLinuxIcon, ColorId::kGrey}},
           {IconType::kPdf, IconParams{kFiletypePdfIcon, ColorId::kRed}},
           {IconType::kPpt,
            IconParams{kFiletypePptIcon, ColorId::kFiletypePpt}},
           {IconType::kScript, IconParams{kFiletypeScriptIcon, ColorId::kBlue}},
           {IconType::kSites,
            IconParams{kFiletypeSitesIcon, ColorId::kFiletypeSites}},
           {IconType::kTini, IconParams{kFiletypeTiniIcon, ColorId::kBlue}},
           {IconType::kVideo, IconParams{kFiletypeVideoIcon, ColorId::kRed}},
           {IconType::kWord, IconParams{kFiletypeWordIcon, ColorId::kBlue}}});
  return *icon_type_to_icon_params;
}

gfx::ImageSkia GetVectorIconFromIconType(IconType icon,
                                         bool dark_background,
                                         absl::optional<int> dip_size) {
  const auto& icon_type_to_icon_params = GetIconTypeToIconParamsMap();
  const auto& it = icon_type_to_icon_params.find(icon);
  DCHECK(it != icon_type_to_icon_params.end());

  const IconParams& params = it->second;
  const gfx::IconDescription description(
      params.icon, dip_size.value_or(kIconDefaultDipSize),
      ResolveColor(params.color_id, dark_background));

  return gfx::CreateVectorIcon(description);
}

}  // namespace

namespace internal {

IconType GetIconTypeForPath(const base::FilePath& filepath) {
  static const base::NoDestructor<base::flat_map<std::string, IconType>>
      // Changes to this map should be reflected in
      // ui/file_manager/file_manager/common/js/file_type.js.
      extension_to_icon({
          // Image
          {".JPEG", IconType::kImage},
          {".JPG", IconType::kImage},
          {".BMP", IconType::kImage},
          {".GIF", IconType::kImage},
          {".ICO", IconType::kImage},
          {".PNG", IconType::kImage},
          {".WEBP", IconType::kImage},
          {".TIFF", IconType::kImage},
          {".TIF", IconType::kImage},
          {".SVG", IconType::kImage},

          // Raw
          {".ARW", IconType::kImage},
          {".CR2", IconType::kImage},
          {".DNG", IconType::kImage},
          {".NEF", IconType::kImage},
          {".NRW", IconType::kImage},
          {".ORF", IconType::kImage},
          {".RAF", IconType::kImage},
          {".RW2", IconType::kImage},

          // Video
          {".3GP", IconType::kVideo},
          {".3GPP", IconType::kVideo},
          {".AVI", IconType::kVideo},
          {".MOV", IconType::kVideo},
          {".MKV", IconType::kVideo},
          {".MP4", IconType::kVideo},
          {".M4V", IconType::kVideo},
          {".MPG", IconType::kVideo},
          {".MPEG", IconType::kVideo},
          {".MPG4", IconType::kVideo},
          {".MPEG4", IconType::kVideo},
          {".OGM", IconType::kVideo},
          {".OGV", IconType::kVideo},
          {".OGX", IconType::kVideo},
          {".WEBM", IconType::kVideo},

          // Audio
          {".AMR", IconType::kAudio},
          {".FLAC", IconType::kAudio},
          {".MP3", IconType::kAudio},
          {".M4A", IconType::kAudio},
          {".OGA", IconType::kAudio},
          {".OGG", IconType::kAudio},
          {".WAV", IconType::kAudio},

          // Text
          {".TXT", IconType::kGeneric},

          // Archive
          {".ZIP", IconType::kArchive},
          {".RAR", IconType::kArchive},
          {".TAR", IconType::kArchive},
          {".TAR.BZ2", IconType::kArchive},
          {".TBZ2", IconType::kArchive},
          {".BZ2", IconType::kArchive},
          {".TBZ", IconType::kArchive},
          {".TAR.GZ", IconType::kArchive},
          {".TGZ", IconType::kArchive},
          {".GZ", IconType::kArchive},
          {".TAR.LZMA", IconType::kArchive},
          {".TLZMA", IconType::kArchive},
          {".LZMA", IconType::kArchive},
          {".TAR.XZ", IconType::kArchive},
          {".TXZ", IconType::kArchive},
          {".XZ", IconType::kArchive},

          // Hosted doc
          {".GDOC", IconType::kGdoc},
          {".GSHEET", IconType::kGsheet},
          {".GSLIDES", IconType::kGslide},
          {".GDRAW", IconType::kGdraw},
          {".GTABLE", IconType::kGtable},
          {".GLINK", IconType::kGeneric},
          {".GFORM", IconType::kGform},
          {".GMAPS", IconType::kGmap},
          {".GSITE", IconType::kGsite},

          // Other
          {".PDF", IconType::kPdf},
          {".HTM", IconType::kGeneric},
          {".HTML", IconType::kGeneric},
          {".MHT", IconType::kGeneric},
          {".MHTM", IconType::kGeneric},
          {".MHTML", IconType::kGeneric},
          {".SHTML", IconType::kGeneric},
          {".XHT", IconType::kGeneric},
          {".XHTM", IconType::kGeneric},
          {".XHTML", IconType::kGeneric},
          {".DOC", IconType::kWord},
          {".DOCX", IconType::kWord},
          {".PPT", IconType::kPpt},
          {".PPTX", IconType::kPpt},
          {".XLS", IconType::kExcel},
          {".XLSX", IconType::kExcel},
          {".TINI", IconType::kTini},
      });

  const auto& icon_it =
      extension_to_icon->find(base::ToUpperASCII(filepath.Extension()));
  if (icon_it != extension_to_icon->end()) {
    return icon_it->second;
  } else {
    return IconType::kGeneric;
  }
}

IconType GetIconTypeFromString(const std::string& icon_type_string) {
  static const base::NoDestructor<std::map<std::string, IconType>>
      type_string_to_icon_type({{"archive", IconType::kArchive},
                                {"audio", IconType::kAudio},
                                {"chart", IconType::kChart},
                                {"excel", IconType::kExcel},
                                {"drive", IconType::kDrive},
                                {"folder", IconType::kFolder},
                                {"gdoc", IconType::kGdoc},
                                {"gdraw", IconType::kGdraw},
                                {"generic", IconType::kGeneric},
                                {"gform", IconType::kGform},
                                {"gmap", IconType::kGmap},
                                {"gsheet", IconType::kGsheet},
                                {"gsite", IconType::kGsite},
                                {"gslides", IconType::kGslide},
                                {"gtable", IconType::kGtable},
                                {"image", IconType::kImage},
                                {"linux", IconType::kLinux},
                                {"pdf", IconType::kPdf},
                                {"ppt", IconType::kPpt},
                                {"script", IconType::kScript},
                                {"shared", IconType::kFolderShared},
                                {"sites", IconType::kSites},
                                {"tini", IconType::kTini},
                                {"video", IconType::kVideo},
                                {"word", IconType::kWord}});

  const auto& icon_it = type_string_to_icon_type->find(icon_type_string);
  if (icon_it != type_string_to_icon_type->end())
    return icon_it->second;
  return IconType::kGeneric;
}

}  // namespace internal

gfx::ImageSkia GetIconForPath(const base::FilePath& filepath,
                              bool dark_background,
                              absl::optional<int> dip_size) {
  return GetVectorIconFromIconType(internal::GetIconTypeForPath(filepath),
                                   dark_background, dip_size);
}

gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath,
                                  bool dark_background) {
  if (!features::IsDarkLightModeEnabled()) {
    // For a chip icon we need to draw 2 icons: a white circle background icon
    // (kFiletypeChipBackgroundIcon) and the icon of the file.
    return gfx::ImageSkiaOperations::CreateSuperimposedImage(
        gfx::CreateVectorIcon(kFiletypeChipBackgroundIcon, kIconDefaultDipSize,
                              SK_ColorWHITE),
        GetVectorIconFromIconType(internal::GetIconTypeForPath(filepath),
                                  /*dark_background=*/false, absl::nullopt));
  }

  return GetIconForPath(filepath, dark_background);
}

gfx::ImageSkia GetIconFromType(const std::string& icon_type,
                               bool dark_background) {
  return GetVectorIconFromIconType(internal::GetIconTypeFromString(icon_type),
                                   dark_background, absl::nullopt);
}

gfx::ImageSkia GetIconFromType(IconType icon_type, bool dark_background) {
  return GetVectorIconFromIconType(icon_type, dark_background, absl::nullopt);
}

SkColor GetIconColorForPath(const base::FilePath& filepath,
                            bool dark_background) {
  const auto& icon_type = internal::GetIconTypeForPath(filepath);
  const auto& icon_type_to_icon_params = GetIconTypeToIconParamsMap();
  const auto& it = icon_type_to_icon_params.find(icon_type);
  DCHECK(it != icon_type_to_icon_params.end());

  return ResolveColor(it->second.color_id, dark_background);
}

}  // namespace chromeos
