// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PhotoCapabilities_h
#define PhotoCapabilities_h

#include "media/capture/mojo/image_capture.mojom-blink.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PhotoCapabilities final
    : public GarbageCollectedFinalized<PhotoCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PhotoCapabilities* Create();
  virtual ~PhotoCapabilities() = default;

  MediaSettingsRange* imageHeight() const { return image_height_; }
  void SetImageHeight(MediaSettingsRange* value) { image_height_ = value; }

  MediaSettingsRange* imageWidth() const { return image_width_; }
  void SetImageWidth(MediaSettingsRange* value) { image_width_ = value; }

  Vector<String> fillLightMode() const;
  void SetFillLightMode(Vector<media::mojom::blink::FillLightMode> modes) {
    fill_light_modes_ = modes;
  }

  String redEyeReduction() const;
  void SetRedEyeReduction(
      media::mojom::blink::RedEyeReduction red_eye_reduction) {
    red_eye_reduction_ = red_eye_reduction;
  }
  bool IsRedEyeReductionControllable() const;

  virtual void Trace(blink::Visitor*);

 private:
  PhotoCapabilities() = default;

  Member<MediaSettingsRange> image_height_;
  Member<MediaSettingsRange> image_width_;
  Vector<media::mojom::blink::FillLightMode> fill_light_modes_;
  media::mojom::blink::RedEyeReduction red_eye_reduction_;
};

}  // namespace blink

#endif  // PhotoCapabilities_h
