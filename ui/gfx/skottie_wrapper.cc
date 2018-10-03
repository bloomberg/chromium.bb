// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skottie_wrapper.h"

#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

SkottieWrapper::SkottieWrapper(
    const scoped_refptr<base::RefCountedMemory>& data_stream) {
  TRACE_EVENT0("ui", "SkottieWrapper Parse");
  SkMemoryStream sk_stream(data_stream->front(), data_stream->size());
  animation_ = skottie::Animation::Make(&sk_stream);
}

SkottieWrapper::SkottieWrapper(std::unique_ptr<SkMemoryStream> stream)
    : animation_(skottie::Animation::Make(stream.get())) {}

SkottieWrapper::~SkottieWrapper() {}

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const gfx::Size& size) {
  SkRect dst = SkRect::MakeXYWH(0, 0, size.width(), size.height());
  {
    base::AutoLock lock(lock_);
    animation_->seek(t);
    animation_->render(canvas, &dst);
  }
}

}  // namespace gfx
