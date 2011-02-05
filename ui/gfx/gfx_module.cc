// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gfx_module.h"

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

}  // namespace gfx
