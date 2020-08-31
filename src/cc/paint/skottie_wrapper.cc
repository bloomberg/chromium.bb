// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_wrapper.h"
#include <vector>

#include "base/hash/hash.h"
#include "base/trace_event/trace_event.h"

namespace cc {

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateSerializable(
    std::vector<uint8_t> data) {
  return base::WrapRefCounted<SkottieWrapper>(
      new SkottieWrapper(std::move(data)));
}

// static
scoped_refptr<SkottieWrapper> SkottieWrapper::CreateNonSerializable(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted<SkottieWrapper>(new SkottieWrapper(data));
}

SkottieWrapper::SkottieWrapper(base::span<const uint8_t> data)
    : animation_(
          skottie::Animation::Make(reinterpret_cast<const char*>(data.data()),
                                   data.size())),
      id_(base::FastHash(data)) {}

SkottieWrapper::SkottieWrapper(std::vector<uint8_t> data)
    : animation_(
          skottie::Animation::Make(reinterpret_cast<const char*>(data.data()),
                                   data.size())),
      raw_data_(std::move(data)),
      id_(base::FastHash(raw_data_)) {}

SkottieWrapper::~SkottieWrapper() = default;

void SkottieWrapper::Draw(SkCanvas* canvas, float t, const SkRect& rect) {
  base::AutoLock lock(lock_);
  animation_->seek(t);
  animation_->render(canvas, &rect);
}

base::span<const uint8_t> SkottieWrapper::raw_data() const {
  DCHECK(raw_data_.size());
  return base::as_bytes(base::make_span(raw_data_.data(), raw_data_.size()));
}

}  // namespace cc
