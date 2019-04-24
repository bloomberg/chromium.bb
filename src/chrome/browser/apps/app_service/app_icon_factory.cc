// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#endif

namespace {

void ApplyIconEffects(apps::IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia) {
  extensions::ChromeAppIcon::ResizeFunction resize_function;
#if defined(OS_CHROMEOS)
  if (icon_effects & apps::IconEffects::kResizeAndPad) {
    resize_function =
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
  }
#endif

  bool apply_chrome_badge = icon_effects & apps::IconEffects::kBadge;
  bool app_launchable = !(icon_effects & apps::IconEffects::kGray);
  bool from_bookmark = icon_effects & apps::IconEffects::kRoundCorners;

  extensions::ChromeAppIcon::ApplyEffects(size_hint_in_dip, resize_function,
                                          apply_chrome_badge, app_launchable,
                                          from_bookmark, image_skia);
}

std::vector<uint8_t> ReadFileAsCompressedData(const base::FilePath path) {
  std::string data;
  base::ReadFileToString(path, &data);
  return std::vector<uint8_t>(data.begin(), data.end());
}

std::vector<uint8_t> CompressedDataFromResource(
    const extensions::ExtensionResource resource) {
  return ReadFileAsCompressedData(resource.GetFilePath());
}

// Runs |callback| passing an IconValuePtr with a compressed image: a
// std::vector<uint8_t>.
void RunCallbackWithCompressedData(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    std::vector<uint8_t> data) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = data.empty()
                             ? apps::mojom::IconCompression::kUnknown
                             : apps::mojom::IconCompression::kCompressed;
  iv->compressed = std::move(data);
  iv->is_placeholder_icon = is_placeholder_icon;
  if (icon_effects) {
    // TODO(crbug.com/826982): decompress the image, apply the icon_effects,
    // and recompress the post-processed image.
    //
    // Even if there are no icon_effects, we might also want to do this if the
    // size_hint_in_dip doesn't match the compressed image's pixel size. This
    // isn't trivial, though, as determining the compressed image's pixel size
    // might involve a sandboxed decoder process.
  }
  std::move(callback).Run(std::move(iv));
}

// Like RunCallbackWithCompressedData, but calls "fallback(callback)" if the
// data is empty.
void RunCallbackWithCompressedDataWithFallback(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)> fallback,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(fallback).Run(std::move(callback));
    return;
  }
  RunCallbackWithCompressedData(size_hint_in_dip, is_placeholder_icon,
                                icon_effects, std::move(callback),
                                std::move(data));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: a
// SkBitmap.
void RunCallbackWithUncompressedSkBitmap(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const SkBitmap& bitmap) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
  iv->is_placeholder_icon = is_placeholder_icon;
  if (icon_effects && !iv->uncompressed.isNull()) {
    ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
  }
  std::move(callback).Run(std::move(iv));
}

// Runs |callback| after converting (in a separate sandboxed process) from a
// std::vector<uint8_t> to a SkBitmap. It calls "fallback(callback)" if the
// data is empty.
void RunCallbackWithCompressedDataToUncompressWithFallback(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)> fallback,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(fallback).Run(std::move(callback));
    return;
  }
  data_decoder::DecodeImage(
      content::ServiceManagerConnection::GetForProcess()->GetConnector(), data,
      data_decoder::mojom::ImageCodec::DEFAULT, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&RunCallbackWithUncompressedSkBitmap, size_hint_in_dip,
                     is_placeholder_icon, icon_effects, std::move(callback)));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: an
// ImageSkia.
void RunCallbackWithUncompressedImageSkia(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::ImageSkia image) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = image;
  iv->is_placeholder_icon = is_placeholder_icon;
  if (icon_effects && !iv->uncompressed.isNull()) {
    ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
  }
  std::move(callback).Run(std::move(iv));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: an
// Image.
void RunCallbackWithUncompressedImage(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::Image& image) {
  RunCallbackWithUncompressedImageSkia(size_hint_in_dip, is_placeholder_icon,
                                       icon_effects, std::move(callback),
                                       image.AsImageSkia());
}

}  // namespace

namespace apps {

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  int size_hint_in_px = apps_util::ConvertDipToPx(size_hint_in_dip);

  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(context)
          ->extension_service()
          ->GetInstalledExtension(extension_id);
  if (extension) {
    extensions::ExtensionResource ext_resource =
        extensions::IconsInfo::GetIconResource(extension, size_hint_in_px,
                                               ExtensionIconSet::MATCH_BIGGER);

    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed: {
        extensions::ImageLoader::Get(context)->LoadImageAsync(
            extension, std::move(ext_resource),
            gfx::Size(size_hint_in_px, size_hint_in_px),
            base::BindOnce(&RunCallbackWithUncompressedImage, size_hint_in_dip,
                           is_placeholder_icon, icon_effects,
                           std::move(callback)));
        return;
      }

      case apps::mojom::IconCompression::kCompressed: {
        base::PostTaskWithTraitsAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&CompressedDataFromResource, ext_resource),
            base::BindOnce(&RunCallbackWithCompressedData, size_hint_in_dip,
                           is_placeholder_icon, icon_effects,
                           std::move(callback)));
        return;
      }
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback) {
  constexpr bool is_placeholder_icon = false;
  switch (icon_compression) {
    case apps::mojom::IconCompression::kUnknown:
      break;

    case apps::mojom::IconCompression::kUncompressed: {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ReadFileAsCompressedData, path),
          base::BindOnce(&RunCallbackWithCompressedDataToUncompressWithFallback,
                         size_hint_in_dip, is_placeholder_icon, icon_effects,
                         std::move(callback), std::move(fallback)));

      return;
    }

    case apps::mojom::IconCompression::kCompressed: {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ReadFileAsCompressedData, path),
          base::BindOnce(&RunCallbackWithCompressedDataWithFallback,
                         size_hint_in_dip, is_placeholder_icon, icon_effects,
                         std::move(callback), std::move(fallback)));
      return;
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback) {
  if (resource_id != 0) {
    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed: {
        gfx::ImageSkia* unscaled =
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                resource_id);
        RunCallbackWithUncompressedImageSkia(
            size_hint_in_dip, is_placeholder_icon, icon_effects,
            std::move(callback),
            gfx::ImageSkiaOperations::CreateResizedImage(
                *unscaled, skia::ImageOperations::RESIZE_BEST,
                gfx::Size(size_hint_in_dip, size_hint_in_dip)));
        return;
      }

      case apps::mojom::IconCompression::kCompressed: {
        base::StringPiece data =
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                resource_id);
        RunCallbackWithCompressedData(
            size_hint_in_dip, is_placeholder_icon, icon_effects,
            std::move(callback),
            std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace apps
