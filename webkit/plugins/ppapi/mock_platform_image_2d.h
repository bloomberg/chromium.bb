// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_MOCK_PLATFORM_IMAGE_2D_H_
#define WEBKIT_PLUGINS_PPAPI_MOCK_PLATFORM_IMAGE_2D_H_

#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "skia/ext/platform_canvas.h"

namespace webkit {
namespace ppapi {

class MockPlatformImage2D : public PluginDelegate::PlatformImage2D {
 public:
  MockPlatformImage2D(int width, int height);
  virtual ~MockPlatformImage2D();

  // PlatformImage2D implementation.
  virtual skia::PlatformCanvas* Map() OVERRIDE;
  virtual intptr_t GetSharedMemoryHandle(uint32* byte_count) const OVERRIDE;
  virtual TransportDIB* GetTransportDIB() const OVERRIDE;

 private:
  int width_;
  int height_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_MOCK_PLATFORM_IMAGE_2D_H_
