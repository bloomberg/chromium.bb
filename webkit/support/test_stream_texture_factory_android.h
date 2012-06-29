// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_SUPPORT_TEST_STREAM_TEXTURE_FACTORY_ANDROID_H_
#define WEBKIT_SUPPORT_TEST_STREAM_TEXTURE_FACTORY_ANDROID_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "webkit/media/android/stream_texture_factory_android.h"

namespace webkit_support {

// An implementation of StreamTextureFactory for tests.
class TestStreamTextureFactory
    : public webkit_media::StreamTextureFactory {
 public:
  TestStreamTextureFactory() { }
  virtual ~TestStreamTextureFactory() { }

  // webkit_media::StreamTextureFactory implementation:
  virtual webkit_media::StreamTextureProxy* CreateProxy() OVERRIDE;

  virtual void EstablishPeer(int stream_id, int player_id) OVERRIDE { }

  virtual unsigned CreateStreamTexture(unsigned* texture_id) OVERRIDE;

  virtual void DestroyStreamTexture(unsigned texture_id) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStreamTextureFactory);
};

}  // namespace webkit_support

#endif  // WEBKIT_SUPPORT_TEST_STREAM_TEXTURE_FACTORY_ANDROID_H_
