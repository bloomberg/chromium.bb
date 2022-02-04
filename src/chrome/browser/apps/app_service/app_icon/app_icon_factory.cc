// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/grit/app_icon_resources.h"
#include "ui/gfx/geometry/skia_conversions.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Copy from Android code, all four sides of the ARC foreground and background
// images are padded 25% of it's width and height.
float kAndroidAdaptiveIconPaddingPercentage = 1.0f / 8.0f;

using SizeToImageSkiaRep = std::map<int, gfx::ImageSkiaRep>;
using ScaleToImageSkiaReps = std::map<float, SizeToImageSkiaRep>;
using MaskImageSkiaReps = std::pair<SkBitmap, ScaleToImageSkiaReps>;

MaskImageSkiaReps& GetMaskResourceIconCache() {
  static base::NoDestructor<MaskImageSkiaReps> mask_cache;
  return *mask_cache;
}

const SkBitmap& GetMaskBitmap() {
  auto& mask_cache = GetMaskResourceIconCache();
  if (mask_cache.first.empty()) {
    // We haven't yet loaded the mask image from resources. Do so and store it
    // in the cache.
    mask_cache.first = *ui::ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_ICON_MASK)
                            .ToSkBitmap();
  }
  DCHECK(!mask_cache.first.empty());
  return mask_cache.first;
}

// Returns the mask image corresponding to the given image |scale| and edge
// pixel |size|. The mask must precisely match the properties of the image it
// will be composited onto.
const gfx::ImageSkiaRep& GetMaskAsImageSkiaRep(float scale,
                                               int size_hint_in_dip) {
  auto& mask_cache = GetMaskResourceIconCache();
  const auto& scale_iter = mask_cache.second.find(scale);
  if (scale_iter != mask_cache.second.end()) {
    const auto& size_iter = scale_iter->second.find(size_hint_in_dip);
    if (size_iter != scale_iter->second.end()) {
      return size_iter->second;
    }
  }

  auto& image_rep = mask_cache.second[scale][size_hint_in_dip];
  image_rep = gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(GetMaskBitmap(),
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    size_hint_in_dip, size_hint_in_dip),
      scale);
  return image_rep;
}

bool IsConsistentPixelSize(const gfx::ImageSkiaRep& rep,
                           const gfx::ImageSkia& image_skia) {
  // The pixel size calculation method must be consistent with
  // ArcAppIconDescriptor::GetSizeInPixels.
  return rep.pixel_width() == roundf(image_skia.width() * rep.scale()) &&
         rep.pixel_height() == roundf(image_skia.height() * rep.scale());
}

// Return whether the image_reps in |image_skia| should be chopped for
// paddings. If the image_rep's pixel size is inconsistent with the scaled width
// and height, that means the image_rep has paddings and should be chopped to
// remove paddings. Otherwise, the image_rep doesn't have padding, and should
// not be chopped.
bool ShouldExtractSubset(const gfx::ImageSkia& image_skia) {
  DCHECK(!image_skia.image_reps().empty());
  for (const auto& rep : image_skia.image_reps()) {
    if (!IsConsistentPixelSize(rep, image_skia)) {
      return true;
    }
  }
  return false;
}

// Chop paddings for all four sides of |image_skia|, and resize image_rep to the
// appropriate size.
gfx::ImageSkia ExtractSubsetForArcImage(const gfx::ImageSkia& image_skia) {
  gfx::ImageSkia subset_image;
  for (const auto& rep : image_skia.image_reps()) {
    if (IsConsistentPixelSize(rep, image_skia)) {
      subset_image.AddRepresentation(rep);
      continue;
    }

    int padding_width =
        rep.pixel_width() * kAndroidAdaptiveIconPaddingPercentage;
    int padding_height =
        rep.pixel_height() * kAndroidAdaptiveIconPaddingPercentage;

    // Chop paddings for all four sides of |image_skia|.
    SkBitmap dst;
    bool success = rep.GetBitmap().extractSubset(
        &dst,
        RectToSkIRect(gfx::Rect(padding_width, padding_height,
                                rep.pixel_width() - 2 * padding_width,
                                rep.pixel_height() - 2 * padding_height)));
    DCHECK(success);

    // Resize |rep| to roundf(size_hint_in_dip * rep.scale()), to keep
    // consistency with ArcAppIconDescriptor::GetSizeInPixels.
    const SkBitmap resized = skia::ImageOperations::Resize(
        dst, skia::ImageOperations::RESIZE_LANCZOS3,
        roundf(image_skia.width() * rep.scale()),
        roundf(image_skia.height() * rep.scale()));
    subset_image.AddRepresentation(gfx::ImageSkiaRep(resized, rep.scale()));
  }
  return subset_image;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

SkBitmap DecompressToSkBitmap(const unsigned char* data, size_t size) {
  base::AssertLongCPUWorkAllowed();
  SkBitmap decoded;
  bool success = gfx::PNGCodec::Decode(data, size, &decoded);
  LOG_IF(ERROR, !success) << "Failed to decode icon data as PNG";
  return decoded;
}

gfx::ImageSkia SkBitmapToImageSkia(SkBitmap bitmap, float icon_scale) {
  return gfx::ImageSkia::CreateFromBitmap(bitmap, icon_scale);
}

// Calls |callback| with the compressed icon |data|.
void CompleteIconWithCompressed(apps::LoadIconCallback callback,
                                std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }
  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = apps::IconType::kCompressed;
  iv->compressed = std::move(data);
  iv->is_placeholder_icon = false;
  std::move(callback).Run(std::move(iv));
}

}  // namespace

namespace apps {

apps::ScaleToSize GetScaleToSize(const gfx::ImageSkia& image_skia) {
  apps::ScaleToSize scale_to_size;
  if (image_skia.image_reps().empty()) {
    scale_to_size[1.0f] = image_skia.size().width();
  } else {
    for (const auto& rep : image_skia.image_reps()) {
      scale_to_size[rep.scale()] = rep.pixel_width();
    }
  }
  return scale_to_size;
}

base::OnceCallback<void(std::vector<uint8_t> compressed_data)>
CompressedDataToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback,
    float icon_scale) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback, float icon_scale,
         std::vector<uint8_t> compressed_data) {
        if (compressed_data.empty()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        // DecompressToSkBitmap is a CPU intensive task that must not run on the
        // UI thread, so post the processing over to the thread pool.
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                [](std::vector<uint8_t> compressed_data, float icon_scale) {
                  return SkBitmapToImageSkia(
                      DecompressToSkBitmap(compressed_data.data(),
                                           compressed_data.size()),
                      icon_scale);
                },
                std::move(compressed_data), icon_scale),
            std::move(callback));
      },
      std::move(callback), icon_scale);
}

std::vector<uint8_t> EncodeImageToPngBytes(const gfx::ImageSkia image,
                                           float rep_icon_scale) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<uint8_t> image_data;

  const gfx::ImageSkiaRep& image_skia_rep =
      image.GetRepresentation(rep_icon_scale);
  if (image_skia_rep.scale() != rep_icon_scale) {
    return image_data;
  }

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (bitmap.drawsNothing()) {
    return image_data;
  }

  base::AssertLongCPUWorkAllowed();
  constexpr bool discard_transparency = false;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                   &image_data);
  if (!success) {
    return std::vector<uint8_t>();
  }
  return image_data;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

gfx::ImageSkia LoadMaskImage(const ScaleToSize& scale_to_size) {
  gfx::ImageSkia mask_image;
  for (const auto& it : scale_to_size) {
    float scale = it.first;
    int size_hint_in_dip = it.second;
    mask_image.AddRepresentation(
        GetMaskAsImageSkiaRep(scale, size_hint_in_dip));
  }

  return mask_image;
}

gfx::ImageSkia ApplyBackgroundAndMask(const gfx::ImageSkia& image) {
  return gfx::ImageSkiaOperations::CreateButtonBackground(
      SK_ColorWHITE, image, LoadMaskImage(GetScaleToSize(image)));
}

gfx::ImageSkia CompositeImagesAndApplyMask(
    const gfx::ImageSkia& foreground_image,
    const gfx::ImageSkia& background_image) {
  bool should_extract_subset_foreground = ShouldExtractSubset(foreground_image);
  bool should_extract_subset_background = ShouldExtractSubset(background_image);

  if (!should_extract_subset_foreground && !should_extract_subset_background) {
    return gfx::ImageSkiaOperations::CreateMaskedImage(
        gfx::ImageSkiaOperations::CreateSuperimposedImage(background_image,
                                                          foreground_image),
        LoadMaskImage(GetScaleToSize(foreground_image)));
  }

  // If the foreground or background image has padding, chop the padding of the
  // four sides, and resize the image_reps for different scales.
  gfx::ImageSkia foreground = should_extract_subset_foreground
                                  ? ExtractSubsetForArcImage(foreground_image)
                                  : foreground_image;
  gfx::ImageSkia background = should_extract_subset_background
                                  ? ExtractSubsetForArcImage(background_image)
                                  : background_image;

  return gfx::ImageSkiaOperations::CreateMaskedImage(
      gfx::ImageSkiaOperations::CreateSuperimposedImage(background, foreground),
      LoadMaskImage(GetScaleToSize(foreground)));
}

void ArcRawIconPngDataToImageSkia(
    arc::mojom::RawIconPngDataPtr icon,
    int size_hint_in_dip,
    base::OnceCallback<void(const gfx::ImageSkia& icon)> callback) {
  if (!icon) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For non-adaptive icons, add the white color background, and apply the mask.
  if (!icon->is_adaptive_icon) {
    if (!icon->icon_png_data.has_value()) {
      std::move(callback).Run(gfx::ImageSkia());
      return;
    }

    scoped_refptr<AppIconLoader> icon_loader =
        base::MakeRefCounted<AppIconLoader>(size_hint_in_dip,
                                            std::move(callback));
    icon_loader->LoadArcIconPngData(icon->icon_png_data.value());
    return;
  }

  if (!icon->foreground_icon_png_data.has_value() ||
      icon->foreground_icon_png_data.value().empty() ||
      !icon->background_icon_png_data.has_value() ||
      icon->background_icon_png_data.value().empty()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // For adaptive icons, composite the background and the foreground images
  // together, then applying the mask
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(size_hint_in_dip,
                                          std::move(callback));
  icon_loader->LoadCompositeImages(
      std::move(icon->foreground_icon_png_data.value()),
      std::move(icon->background_icon_png_data.value()));
}

void ArcActivityIconsToImageSkias(
    const std::vector<arc::mojom::ActivityIconPtr>& icons,
    base::OnceCallback<void(const std::vector<gfx::ImageSkia>& icons)>
        callback) {
  if (icons.empty()) {
    std::move(callback).Run(std::vector<gfx::ImageSkia>{});
    return;
  }

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(std::move(callback));
  icon_loader->LoadArcActivityIcons(icons);
}

gfx::ImageSkia ConvertSquareBitmapsToImageSkia(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps,
    IconEffects icon_effects,
    int size_hint_in_dip) {
  auto image_skia =
      ConvertIconBitmapsToImageSkia(icon_bitmaps, size_hint_in_dip);

  if (image_skia.isNull()) {
    return gfx::ImageSkia{};
  }

  if ((icon_effects & IconEffects::kCrOsStandardMask) &&
      (icon_effects & IconEffects::kCrOsStandardBackground)) {
    image_skia = apps::ApplyBackgroundAndMask(image_skia);
  }
  return image_skia;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

gfx::ImageSkia ConvertIconBitmapsToImageSkia(
    const std::map<SquareSizePx, SkBitmap>& icon_bitmaps,
    int size_hint_in_dip) {
  if (icon_bitmaps.empty()) {
    return gfx::ImageSkia{};
  }

  gfx::ImageSkia image_skia;
  auto it = icon_bitmaps.begin();

  for (ui::ResourceScaleFactor scale_factor :
       ui::GetSupportedResourceScaleFactors()) {
    float icon_scale = ui::GetScaleForResourceScaleFactor(scale_factor);

    SquareSizePx icon_size_in_px =
        gfx::ScaleToFlooredSize(gfx::Size(size_hint_in_dip, size_hint_in_dip),
                                icon_scale)
            .width();

    while (it != icon_bitmaps.end() && it->first < icon_size_in_px) {
      ++it;
    }

    if (it == icon_bitmaps.end() || it->second.empty()) {
      continue;
    }

    SkBitmap bitmap = it->second;
    // Resize |bitmap| to match |icon_scale|.
    //
    // TODO(crbug.com/1189994): All conversions in app_icon_factory.cc must
    // perform CPU-heavy operations off the Browser UI thread.
    if (bitmap.width() != icon_size_in_px) {
      bitmap = skia::ImageOperations::Resize(
          bitmap, skia::ImageOperations::RESIZE_LANCZOS3, icon_size_in_px,
          icon_size_in_px);
    }

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, icon_scale));
  }

  if (image_skia.isNull()) {
    return gfx::ImageSkia{};
  }

  image_skia.EnsureRepsForSupportedScales();

  return image_skia;
}

void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      IconValuePtr iv,
                      LoadIconCallback callback) {
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(size_hint_in_dip,
                                          std::move(callback));
  icon_loader->ApplyIconEffects(icon_effects, std::move(iv));
}

void ConvertUncompressedIconToCompressedIcon(IconValuePtr iv,
                                             LoadIconCallback callback) {
  iv->uncompressed.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&apps::EncodeImageToPngBytes, iv->uncompressed,
                     /*rep_icon_scale=*/1.0f),
      base::BindOnce(&CompleteIconWithCompressed, std::move(callback)));
}

void LoadIconFromExtension(IconType icon_type,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr bool is_placeholder_icon = false;
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadExtensionIcon(
      extensions::ExtensionRegistry::Get(context)->GetInstalledExtension(
          extension_id),
      context);
}

void LoadIconFromWebApp(content::BrowserContext* context,
                        IconType icon_type,
                        int size_hint_in_dip,
                        const std::string& web_app_id,
                        IconEffects icon_effects,
                        LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(
          Profile::FromBrowserContext(context));

  DCHECK(web_app_provider);
  constexpr bool is_placeholder_icon = false;
  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadWebAppIcon(
      web_app_id, web_app_provider->registrar().GetAppStartUrl(web_app_id),
      web_app_provider->icon_manager(), Profile::FromBrowserContext(context));
}

void LoadIconFromFileWithFallback(
    IconType icon_type,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    LoadIconCallback callback,
    base::OnceCallback<void(LoadIconCallback)> fallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(fallback), std::move(callback));
  icon_loader->LoadCompressedIconFromFile(path);
}

void LoadIconFromCompressedData(IconType icon_type,
                                int size_hint_in_dip,
                                IconEffects icon_effects,
                                const std::string& compressed_icon_data,
                                LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          kInvalidIconResource, std::move(callback));
  icon_loader->LoadIconFromCompressedData(compressed_icon_data);
}

void LoadIconFromResource(IconType icon_type,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no fallback icon for a resource.
  constexpr int fallback_icon_resource = 0;

  scoped_refptr<AppIconLoader> icon_loader =
      base::MakeRefCounted<AppIconLoader>(
          icon_type, size_hint_in_dip, is_placeholder_icon, icon_effects,
          fallback_icon_resource, std::move(callback));
  icon_loader->LoadIconFromResource(resource_id);
}

}  // namespace apps
