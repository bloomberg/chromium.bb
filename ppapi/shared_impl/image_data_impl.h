// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_IMAGE_DATA_IMPL_H_
#define PPAPI_SHARED_IMPL_IMAGE_DATA_IMPL_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/ppb_image_data.h"

namespace pp {
namespace shared_impl {

// Contains the implementation of some simple image data functions that are
// shared between the proxy and Chrome's implementation. Since these functions
// are just lists of what we support, it's much easier to just have the same
// code run in the plugin process than to proxy it.
//
// It's possible the implementation will get more complex. In this case, it's
// probably best to have some kind of "configuration" message that the renderer
// sends to the plugin process on startup that contains all of these kind of
// settings.
class ImageDataImpl {
 public:
  static PP_ImageDataFormat GetNativeImageDataFormat();
  static bool IsImageDataFormatSupported(PP_ImageDataFormat format);
};

}  // namespace shared_impl
}  // namespace pp

#endif  // PPAPI_SHARED_IMPL_IMAGE_DATA_IMPL_H_
