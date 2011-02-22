// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image.h"

namespace ui {

namespace {

// Font sizes relative to base font.
#if defined(OS_CHROMEOS) && defined(CROS_FONTS_USING_BCI)
const int kSmallFontSizeDelta = -3;
const int kMediumFontSizeDelta = 2;
const int kLargeFontSizeDelta = 7;
#else
const int kSmallFontSizeDelta = -2;
const int kMediumFontSizeDelta = 3;
const int kLargeFontSizeDelta = 8;
#endif

}  // namespace

ResourceBundle* ResourceBundle::g_shared_instance_ = NULL;

/* static */
// TODO(glen): Finish moving these into theme provider (dialogs still
//    depend on these colors).
const SkColor ResourceBundle::frame_color =
     SkColorSetRGB(66, 116, 201);
const SkColor ResourceBundle::frame_color_inactive =
     SkColorSetRGB(161, 182, 228);
const SkColor ResourceBundle::frame_color_app_panel =
     SK_ColorWHITE;
const SkColor ResourceBundle::frame_color_app_panel_inactive =
     SK_ColorWHITE;
const SkColor ResourceBundle::frame_color_incognito =
     SkColorSetRGB(83, 106, 139);
const SkColor ResourceBundle::frame_color_incognito_inactive =
     SkColorSetRGB(126, 139, 156);
const SkColor ResourceBundle::toolbar_color =
     SkColorSetRGB(210, 225, 246);
const SkColor ResourceBundle::toolbar_separator_color =
     SkColorSetRGB(182, 186, 192);

/* static */
std::string ResourceBundle::InitSharedInstance(
    const std::string& pref_locale) {
  DCHECK(g_shared_instance_ == NULL) << "ResourceBundle initialized twice";
  g_shared_instance_ = new ResourceBundle();

  g_shared_instance_->LoadCommonResources();
  return g_shared_instance_->LoadLocaleResources(pref_locale);
}

/* static */
std::string ResourceBundle::ReloadSharedInstance(
    const std::string& pref_locale) {
  DCHECK(g_shared_instance_ != NULL) << "ResourceBundle not initialized";

  g_shared_instance_->UnloadLocaleResources();
  return g_shared_instance_->LoadLocaleResources(pref_locale);
}

/* static */
void ResourceBundle::AddDataPackToSharedInstance(const FilePath& path) {
  DCHECK(g_shared_instance_ != NULL) << "ResourceBundle not initialized";
  g_shared_instance_->data_packs_.push_back(new LoadedDataPack(path));
}

/* static */
void ResourceBundle::CleanupSharedInstance() {
  if (g_shared_instance_) {
    delete g_shared_instance_;
    g_shared_instance_ = NULL;
  }
}

/* static */
ResourceBundle& ResourceBundle::GetSharedInstance() {
  // Must call InitSharedInstance before this function.
  CHECK(g_shared_instance_ != NULL);
  return *g_shared_instance_;
}

SkBitmap* ResourceBundle::GetBitmapNamed(int resource_id) {
  const SkBitmap* bitmap =
      static_cast<const SkBitmap*>(GetImageNamed(resource_id));
  return const_cast<SkBitmap*>(bitmap);
}

gfx::Image& ResourceBundle::GetImageNamed(int resource_id) {
  // Check to see if the image is already in the cache.
  {
    base::AutoLock lock_scope(*lock_);
    ImageMap::const_iterator found = images_.find(resource_id);
    if (found != images_.end())
      return *found->second;
  }

  scoped_ptr<SkBitmap> bitmap(LoadBitmap(resources_data_, resource_id));
  if (bitmap.get()) {
    // The load was successful, so cache the image.
    base::AutoLock lock_scope(*lock_);

    // Another thread raced the load and has already cached the image.
    if (images_.count(resource_id))
      return *images_[resource_id];

    gfx::Image* image = new gfx::Image(bitmap.release());
    images_[resource_id] = image;
    return *image;
  }

  // The load failed to retrieve the image; show a debugging red square.
  LOG(WARNING) << "Unable to load image with id " << resource_id;
  NOTREACHED();  // Want to assert in debug mode.
  return *GetEmptyImage();
}

#if !defined(OS_MACOSX) && !defined(OS_LINUX)
// Only Mac and Linux have non-Skia native image types. All other platforms use
// Skia natively, so just use GetImageNamed().
gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return GetImageNamed(resource_id);
}
#endif

RefCountedStaticMemory* ResourceBundle::LoadDataResourceBytes(
    int resource_id) const {
  RefCountedStaticMemory* bytes =
      LoadResourceBytes(resources_data_, resource_id);

  // Check all our additional data packs for the resources if it wasn't loaded
  // from our main source.
  for (std::vector<LoadedDataPack*>::const_iterator it = data_packs_.begin();
       !bytes && it != data_packs_.end(); ++it) {
    bytes = (*it)->GetStaticMemory(resource_id);
  }

  return bytes;
}

const gfx::Font& ResourceBundle::GetFont(FontStyle style) {
  LoadFontsIfNecessary();
  switch (style) {
    case BoldFont:
      return *bold_font_;
    case SmallFont:
      return *small_font_;
    case MediumFont:
      return *medium_font_;
    case MediumBoldFont:
      return *medium_bold_font_;
    case LargeFont:
      return *large_font_;
    default:
      return *base_font_;
  }
}

ResourceBundle::ResourceBundle()
    : lock_(new base::Lock),
      resources_data_(NULL),
      locale_resources_data_(NULL) {
}

void ResourceBundle::FreeImages() {
  STLDeleteContainerPairSecondPointers(images_.begin(),
                                       images_.end());
  images_.clear();
}

void ResourceBundle::LoadFontsIfNecessary() {
  base::AutoLock lock_scope(*lock_);
  if (!base_font_.get()) {
    base_font_.reset(new gfx::Font());

    bold_font_.reset(new gfx::Font());
    *bold_font_ =
        base_font_->DeriveFont(0, base_font_->GetStyle() | gfx::Font::BOLD);

    small_font_.reset(new gfx::Font());
    *small_font_ = base_font_->DeriveFont(kSmallFontSizeDelta);

    medium_font_.reset(new gfx::Font());
    *medium_font_ = base_font_->DeriveFont(kMediumFontSizeDelta);

    medium_bold_font_.reset(new gfx::Font());
    *medium_bold_font_ =
        base_font_->DeriveFont(kMediumFontSizeDelta,
                               base_font_->GetStyle() | gfx::Font::BOLD);

    large_font_.reset(new gfx::Font());
    *large_font_ = base_font_->DeriveFont(kLargeFontSizeDelta);
  }
}

/* static */
SkBitmap* ResourceBundle::LoadBitmap(DataHandle data_handle, int resource_id) {
  scoped_refptr<RefCountedMemory> memory(
      LoadResourceBytes(data_handle, resource_id));
  if (!memory)
    return NULL;

  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(memory->front(), memory->size(), &bitmap)) {
    NOTREACHED() << "Unable to decode theme image resource " << resource_id;
    return NULL;
  }

  return new SkBitmap(bitmap);
}

gfx::Image* ResourceBundle::GetEmptyImage() {
  base::AutoLock lock(*lock_);

  static gfx::Image* empty_image = NULL;
  if (!empty_image) {
    // The placeholder bitmap is bright red so people notice the problem.
    // This bitmap will be leaked, but this code should never be hit.
    SkBitmap* bitmap = new SkBitmap();
    bitmap->setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
    bitmap->allocPixels();
    bitmap->eraseARGB(255, 255, 0, 0);
    empty_image = new gfx::Image(bitmap);
  }
  return empty_image;
}

// LoadedDataPack -------------------------------------------------------------

ResourceBundle::LoadedDataPack::LoadedDataPack(const FilePath& path)
    : path_(path) {
  // Always preload the data packs so we can maintain constness.
  Load();
}

ResourceBundle::LoadedDataPack::~LoadedDataPack() {
}

void ResourceBundle::LoadedDataPack::Load() {
  DCHECK(!data_pack_.get());
  data_pack_.reset(new ui::DataPack);
  bool success = data_pack_->Load(path_);
  LOG_IF(ERROR, !success) << "Failed to load " << path_.value()
      << "\nYou will not be able to use the Bookmarks Manager or "
      << "about:net-internals.";
}

bool ResourceBundle::LoadedDataPack::GetStringPiece(
    int resource_id, base::StringPiece* data) const {
  return data_pack_->GetStringPiece(static_cast<uint32>(resource_id), data);
}

RefCountedStaticMemory* ResourceBundle::LoadedDataPack::GetStaticMemory(
    int resource_id) const {
  return data_pack_->GetStaticMemory(resource_id);
}

}  // namespace ui
