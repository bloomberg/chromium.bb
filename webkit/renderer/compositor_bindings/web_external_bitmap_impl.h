// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebExternalBitmap.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace base {
class SharedMemory;
}

namespace webkit {

typedef scoped_ptr<base::SharedMemory> (*SharedMemoryAllocationFunction)(
    size_t);

// Sets the function that this will use to allocate shared memory.
WEBKIT_COMPOSITOR_BINDINGS_EXPORT void SetSharedMemoryAllocationFunction(
    SharedMemoryAllocationFunction);

class WebExternalBitmapImpl : public WebKit::WebExternalBitmap {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebExternalBitmapImpl();
  virtual ~WebExternalBitmapImpl();

  // WebKit::WebExternalBitmap implementation.
  virtual WebKit::WebSize size() OVERRIDE;
  virtual void setSize(WebKit::WebSize size) OVERRIDE;
  virtual uint8* pixels() OVERRIDE;

  base::SharedMemory* shared_memory() { return shared_memory_.get(); }

 private:
  scoped_ptr<base::SharedMemory> shared_memory_;
  WebKit::WebSize size_;

  DISALLOW_COPY_AND_ASSIGN(WebExternalBitmapImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_EXTERNAL_BITMAP_IMPL_H_
