// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaSettingsRange_h
#define MediaSettingsRange_h

#include "media/capture/mojo/image_capture.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class MediaSettingsRange final : public GarbageCollected<MediaSettingsRange>,
                                 public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MediaSettingsRange* Create(double max, double min, double step) {
    return new MediaSettingsRange(max, min, step);
  }
  static MediaSettingsRange* Create(media::mojom::blink::RangePtr range) {
    return MediaSettingsRange::Create(*range);
  }
  static MediaSettingsRange* Create(const media::mojom::blink::Range& range) {
    return MediaSettingsRange::Create(range.max, range.min, range.step);
  }

  double max() const { return max_; }
  double min() const { return min_; }
  double step() const { return step_; }

  void Trace(blink::Visitor* visitor) {}

 private:
  MediaSettingsRange(double max, double min, double step)
      : max_(max), min_(min), step_(step) {}

  double max_;
  double min_;
  double step_;
};

}  // namespace blink

#endif  // MediaSettingsRange_h
