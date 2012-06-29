// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/test_stream_texture_factory_android.h"

namespace webkit_support {

webkit_media::StreamTextureProxy* TestStreamTextureFactory::CreateProxy() {
  return NULL;
}

unsigned TestStreamTextureFactory::CreateStreamTexture(unsigned* texture_id) {
  texture_id = 0;
  return 0;
}

void TestStreamTextureFactory::DestroyStreamTexture(unsigned texture_id) {
}

}  // namespace webkit_support
