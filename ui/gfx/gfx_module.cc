// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gfx_module.h"

#include <map>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace gfx {

static GfxModule::ResourceProvider resource_provider = NULL;

// static
void GfxModule::SetResourceProvider(ResourceProvider func) {
  resource_provider = func;
}

// static
base::StringPiece GfxModule::GetResource(int key) {
  return resource_provider ? resource_provider(key) : base::StringPiece();
}

// TODO(xiyuan): Use ResourceBundle to have shared cache when it is in ui_base.
// static
SkBitmap* GfxModule::GetBitmapNamed(int key) {
  typedef std::map<int, SkBitmap*> SkImageMap;
  static SkImageMap images;  // Image resource cache.

  SkImageMap::const_iterator found = images.find(key);
  if (found != images.end())
    return found->second;

  base::StringPiece data = GetResource(key);
  if (!data.size()) {
    NOTREACHED() << "Unable to load image resource " << key;
    return NULL;
  }

  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(data.data()),
      data.size(), bitmap.get())) {
    NOTREACHED() << "Unable to decode image resource " << key;
    return NULL;
  }

  images[key] = bitmap.get();
  return bitmap.release();
}

}  // namespace gfx
