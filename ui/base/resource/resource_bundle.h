// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
#pragma once

#include "build/build_config.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/native_widget_types.h"

class SkBitmap;

namespace base {
class Lock;
class RefCountedStaticMemory;
}

namespace gfx {
class Font;
class Image;
}

namespace ui {

class ResourceHandle;

// ResourceBundle is a central facility to load images and other resources,
// such as theme graphics. Every resource is loaded only once.
class UI_EXPORT ResourceBundle {
 public:
  // An enumeration of the various font styles used throughout Chrome.
  // The following holds true for the font sizes:
  // Small <= Base <= Bold <= Medium <= MediumBold <= Large.
  enum FontStyle {
    SmallFont,
    BaseFont,
    BoldFont,
    MediumFont,
    // NOTE: depending upon the locale, this may *not* result in a bold font.
    MediumBoldFont,
    LargeFont,
    // NOTE: depending upon the locale, this may *not* result in a bold font.
    LargeBoldFont,
  };

  enum ImageRTL {
    // Images are flipped in RTL locales.
    RTL_ENABLED,
    // Images are never flipped.
    RTL_DISABLED,
  };

  // Initialize the ResourceBundle for this process.  Returns the language
  // selected.
  // NOTE: Mac ignores this and always loads up resources for the language
  // defined by the Cocoa UI (i.e., NSBundle does the language work).
  static std::string InitSharedInstanceWithLocale(
      const std::string& pref_locale);

  // Initialize the ResourceBundle using given data pack path for testing.
  static void InitSharedInstanceWithPakFile(const FilePath& path);

  // Delete the ResourceBundle for this process if it exists.
  static void CleanupSharedInstance();

  // Returns true after the global resource loader instance has been created.
  static bool HasSharedInstance();

  // Return the global resource loader instance.
  static ResourceBundle& GetSharedInstance();

  // Check if the .pak for the given locale exists.
  static bool LocaleDataPakExists(const std::string& locale);

  // Registers additional data pack files with the global ResourceBundle.  When
  // looking for a DataResource, we will search these files after searching the
  // main module. |scale_factor| is the scale of images in this resource pak
  // relative to the images in the 1x resource pak. This method is not thread
  // safe! You should call it immediately after calling InitSharedInstance.
  void AddDataPack(const FilePath& path, float scale_factor);

  // Changes the locale for an already-initialized ResourceBundle, returning the
  // name of the newly-loaded locale.  Future calls to get strings will return
  // the strings for this new locale.  This has no effect on existing or future
  // image resources.  |locale_resources_data_| is protected by a lock for the
  // duration of the swap, as GetLocalizedString() may be concurrently invoked
  // on another thread.
  std::string ReloadLocaleResources(const std::string& pref_locale);

  // Gets the bitmap with the specified resource_id from the current module
  // data. Returns a pointer to a shared instance of the SkBitmap. This shared
  // bitmap is owned by the resource bundle and should not be freed.
  //
  // !! THIS IS DEPRECATED. PLEASE USE THE METHOD BELOW. !!
  SkBitmap* GetBitmapNamed(int resource_id);

  // Gets an image resource from the current module data. This will load the
  // image in Skia format by default. The ResourceBundle owns this.
  gfx::Image& GetImageNamed(int resource_id);

  // Similar to GetImageNamed, but rather than loading the image in Skia format,
  // it will load in the native platform type. This can avoid conversion from
  // one image type to another. ResourceBundle owns the result.
  //
  // Note that if the same resource has already been loaded in GetImageNamed(),
  // gfx::Image will perform a conversion, rather than using the native image
  // loading code of ResourceBundle.
  //
  // If |rtl| is RTL_ENABLED then the image is flipped in RTL locales.
  gfx::Image& GetNativeImageNamed(int resource_id, ImageRTL rtl);

  // Same as GetNativeImageNamed() except that RTL is not enabled.
  gfx::Image& GetNativeImageNamed(int resource_id);

  // Loads the raw bytes of a data resource into |bytes|,
  // without doing any processing or interpretation of
  // the resource. Returns whether we successfully read the resource.
  base::RefCountedStaticMemory* LoadDataResourceBytes(int resource_id) const;

  // Return the contents of a resource in a StringPiece given the resource id.
  base::StringPiece GetRawDataResource(int resource_id) const;

  // Get a localized string given a message id.  Returns an empty
  // string if the message_id is not found.
  string16 GetLocalizedString(int message_id);

  // Returns the font for the specified style.
  const gfx::Font& GetFont(FontStyle style);

  // Resets and reloads the cached fonts.  This is useful when the fonts of the
  // system have changed, for example, when the locale has changed.
  void ReloadFonts();

  // Overrides the path to the pak file from which the locale resources will be
  // loaded. Pass an empty path to undo.
  void OverrideLocalePakForTest(const FilePath& pak_path);

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceBundle, LoadDataResourceBytes);

  // Ctor/dtor are private, since we're a singleton.
  ResourceBundle();
  ~ResourceBundle();

  // Free skia_images_.
  void FreeImages();

  // Load the main resources.
  void LoadCommonResources();

  // Try to load the locale specific strings from an external data module.
  // Returns the locale that is loaded.
  std::string LoadLocaleResources(const std::string& pref_locale);

  // Load test resources in given path.
  void LoadTestResources(const FilePath& path);

  // Unload the locale specific strings and prepares to load new ones. See
  // comments for ReloadLocaleResources().
  void UnloadLocaleResources();

  // Initialize all the gfx::Font members if they haven't yet been initialized.
  void LoadFontsIfNecessary();

  // Returns the full pathname of the locale file to load.  May return an empty
  // string if no locale data files are found.
  static FilePath GetLocaleFilePath(const std::string& app_locale);

  // Creates and returns a new SkBitmap given the data file to look in and the
  // resource id.  It's up to the caller to free the returned bitmap when
  // done.
  SkBitmap* LoadBitmap(const ResourceHandle& dll_inst, int resource_id);

  // Returns an empty image for when a resource cannot be loaded. This is a
  // bright red bitmap.
  gfx::Image* GetEmptyImage();

  const FilePath& GetOverriddenPakPath();

  // Protects |images_| and font-related members.
  scoped_ptr<base::Lock> images_and_fonts_lock_;

  // Protects |locale_resources_data_|.
  scoped_ptr<base::Lock> locale_resources_data_lock_;

  // Handles for data sources.
  scoped_ptr<ResourceHandle> locale_resources_data_;
  ScopedVector<ResourceHandle> data_packs_;

  // Cached images. The ResourceBundle caches all retrieved images and keeps
  // ownership of the pointers.
  typedef std::map<int, gfx::Image*> ImageMap;
  ImageMap images_;

  // The various fonts used. Cached to avoid repeated GDI creation/destruction.
  scoped_ptr<gfx::Font> base_font_;
  scoped_ptr<gfx::Font> bold_font_;
  scoped_ptr<gfx::Font> small_font_;
  scoped_ptr<gfx::Font> medium_font_;
  scoped_ptr<gfx::Font> medium_bold_font_;
  scoped_ptr<gfx::Font> large_font_;
  scoped_ptr<gfx::Font> large_bold_font_;
  scoped_ptr<gfx::Font> web_font_;

  static ResourceBundle* g_shared_instance_;

  FilePath overridden_pak_path_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundle);
};

}  // namespace ui

// TODO(beng): Someday, maybe, get rid of this.
using ui::ResourceBundle;

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_H_
