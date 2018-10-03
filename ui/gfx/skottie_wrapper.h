// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKOTTIE_WRAPPER_H_
#define UI_GFX_SKOTTIE_WRAPPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/modules/skottie/include/Skottie.h"
#include "ui/gfx/gfx_export.h"

class SkCanvas;
class SkMemoryStream;

namespace base {
class RefCountedMemory;
}  // namespace base

namespace gfx {
class Size;

// A wrapper over Skia's Skottie object that can be shared by multiple
// SkiaVectorAnimation objects. This class is thread safe when performing a draw
// on an SkCanvas.
class GFX_EXPORT SkottieWrapper
    : public base::RefCountedThreadSafe<SkottieWrapper> {
 public:
  explicit SkottieWrapper(
      const scoped_refptr<base::RefCountedMemory>& data_stream);
  explicit SkottieWrapper(std::unique_ptr<SkMemoryStream> stream);

  // A thread safe call that will draw an image of size |size| for the frame at
  // normalized time instant |t| onto the |canvas|.
  void Draw(SkCanvas* canvas, float t, const Size& size);

  float duration() const { return animation_->duration(); }
  SkSize size() const { return animation_->size(); }

 private:
  friend class base::RefCountedThreadSafe<SkottieWrapper>;
  ~SkottieWrapper();

  base::Lock lock_;
  sk_sp<skottie::Animation> animation_;

  DISALLOW_COPY_AND_ASSIGN(SkottieWrapper);
};

}  // namespace gfx

#endif  // UI_GFX_SKOTTIE_WRAPPER_H_
