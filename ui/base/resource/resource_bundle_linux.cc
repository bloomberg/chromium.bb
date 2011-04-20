// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include <gtk/gtk.h>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/font.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image.h"

namespace ui {

namespace {

// Convert the raw image data into a GdkPixbuf.  The GdkPixbuf that is returned
// has a ref count of 1 so the caller must call g_object_unref to free the
// memory.
GdkPixbuf* LoadPixbuf(RefCountedStaticMemory* data, bool rtl_enabled) {
  ScopedGObject<GdkPixbufLoader>::Type loader(gdk_pixbuf_loader_new());
  bool ok = data && gdk_pixbuf_loader_write(loader.get(),
      reinterpret_cast<const guint8*>(data->front()), data->size(), NULL);
  if (!ok)
    return NULL;
  // Calling gdk_pixbuf_loader_close forces the data to be parsed by the
  // loader.  We must do this before calling gdk_pixbuf_loader_get_pixbuf.
  ok = gdk_pixbuf_loader_close(loader.get(), NULL);
  if (!ok)
    return NULL;
  GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader.get());
  if (!pixbuf)
    return NULL;

  if (base::i18n::IsRTL() && rtl_enabled) {
    // |pixbuf| will get unreffed and destroyed (see below). The returned value
    // has ref count 1.
    return gdk_pixbuf_flip(pixbuf, TRUE);
  } else {
    // The pixbuf is owned by the loader, so add a ref so when we delete the
    // loader (when the ScopedGObject goes out of scope), the pixbuf still
    // exists.
    g_object_ref(pixbuf);
    return pixbuf;
  }
}

}  // namespace

// static
FilePath ResourceBundle::GetResourcesFilePath() {
  FilePath resources_file_path;
  PathService::Get(ui::FILE_RESOURCES_PAK, &resources_file_path);
  return resources_file_path;
}

// static
FilePath ResourceBundle::GetLocaleFilePath(const std::string& app_locale) {
  FilePath locale_file_path;
  PathService::Get(ui::DIR_LOCALES, &locale_file_path);
  if (locale_file_path.empty())
    return locale_file_path;
  if (app_locale.empty())
    return FilePath();
  locale_file_path = locale_file_path.AppendASCII(app_locale + ".pak");
  if (!file_util::PathExists(locale_file_path))
    return FilePath();
  return locale_file_path;
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id) {
  return *GetPixbufImpl(resource_id, false);
}

gfx::Image* ResourceBundle::GetPixbufImpl(int resource_id, bool rtl_enabled) {
  // Use the negative |resource_id| for the key for BIDI-aware images.
  int key = rtl_enabled ? -resource_id : resource_id;

  // Check to see if the image is already in the cache.
  {
    base::AutoLock lock_scope(*lock_);
    ImageMap::const_iterator found = images_.find(key);
    if (found != images_.end())
      return found->second;
  }

  scoped_refptr<RefCountedStaticMemory> data(
      LoadDataResourceBytes(resource_id));
  GdkPixbuf* pixbuf = LoadPixbuf(data.get(), rtl_enabled);

  // The load was successful, so cache the image.
  if (pixbuf) {
    base::AutoLock lock_scope(*lock_);

    // Another thread raced the load and has already cached the image.
    if (images_.count(key)) {
      g_object_unref(pixbuf);
      return images_[key];
    }

    gfx::Image* image = new gfx::Image(pixbuf);  // Takes ownership.
    images_[key] = image;
    return image;
  }

  LOG(WARNING) << "Unable to pixbuf with id " << resource_id;
  NOTREACHED();  // Want to assert in debug mode.
  return GetEmptyImage();
}

GdkPixbuf* ResourceBundle::GetPixbufNamed(int resource_id) {
  return *GetPixbufImpl(resource_id, false);
}

GdkPixbuf* ResourceBundle::GetRTLEnabledPixbufNamed(int resource_id) {
  return *GetPixbufImpl(resource_id, true);
}

}  // namespace ui
