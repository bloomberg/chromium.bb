// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_

#include "base/memory/ref_counted.h"

namespace ui {

// This represents a buffer that can be directly imported via GL for
// rendering, or exported via dma-buf fds.
class NativePixmap : public base::RefCounted<NativePixmap> {
 public:
  NativePixmap() {}

  virtual void* /* EGLClientBuffer */ GetEGLClientBuffer() = 0;
  virtual int GetDmaBufFd() = 0;

 protected:
  virtual ~NativePixmap() {}

  friend class base::RefCounted<NativePixmap>;

  DISALLOW_COPY_AND_ASSIGN(NativePixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_H_
