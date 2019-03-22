// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

float GetDeviceScaleFactor() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return 1.0f;
  }
  return screen->GetPrimaryDisplay().device_scale_factor();
}

int ConvertDipToPx(int dip) {
  return base::saturated_cast<int>(
      std::floor(static_cast<float>(dip) * GetDeviceScaleFactor()));
}

std::vector<uint8_t> ReadExtensionResource(
    extensions::ExtensionResource ext_resource) {
  std::string data;
  base::ReadFileToString(ext_resource.GetFilePath(), &data);
  return std::vector<uint8_t>(data.begin(), data.end());
}

// Runs |callback| passing an IconValuePtr with a compressed image.
void RunCallbackWithCompressedData(
    apps::mojom::Publisher::LoadIconCallback callback,
    std::vector<uint8_t> data) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = data.empty()
                             ? apps::mojom::IconCompression::kUnknown
                             : apps::mojom::IconCompression::kCompressed;
  iv->compressed = std::move(data);
  std::move(callback).Run(std::move(iv));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image.
void RunCallbackWithUncompressedImageSkia(
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::ImageSkia image) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = image;
  std::move(callback).Run(std::move(iv));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image.
void RunCallbackWithUncompressedImage(
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::Image& image) {
  RunCallbackWithUncompressedImageSkia(std::move(callback),
                                       image.AsImageSkia());
}

}  // namespace

namespace apps {

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           apps::mojom::Publisher::LoadIconCallback callback,
                           content::BrowserContext* context,
                           const std::string& extension_id) {
  int size_hint_in_px = ConvertDipToPx(size_hint_in_dip);

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

      case apps::mojom::IconCompression::kUncompressed:
        extensions::ImageLoader::Get(context)->LoadImageAsync(
            extension, std::move(ext_resource),
            gfx::Size(size_hint_in_px, size_hint_in_px),
            base::BindOnce(&RunCallbackWithUncompressedImage,
                           std::move(callback)));
        return;

      case apps::mojom::IconCompression::kCompressed:
        base::PostTaskWithTraitsAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&ReadExtensionResource, std::move(ext_resource)),
            base::BindOnce(&RunCallbackWithCompressedData,
                           std::move(callback)));
        return;
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          apps::mojom::Publisher::LoadIconCallback callback,
                          int resource_id) {
  if (resource_id != 0) {
    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed: {
        gfx::ImageSkia* unscaled =
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                resource_id);
        RunCallbackWithUncompressedImageSkia(
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
            std::move(callback),
            std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace apps
