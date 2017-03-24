// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PhotoCapabilities_h
#define PhotoCapabilities_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "media/capture/mojo/image_capture.mojom-blink.h"
#include "modules/imagecapture/MediaSettingsRange.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PhotoCapabilities final
    : public GarbageCollectedFinalized<PhotoCapabilities>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PhotoCapabilities* create();
  virtual ~PhotoCapabilities() = default;

  MediaSettingsRange* imageHeight() const { return m_imageHeight; }
  void setImageHeight(MediaSettingsRange* value) { m_imageHeight = value; }

  MediaSettingsRange* imageWidth() const { return m_imageWidth; }
  void setImageWidth(MediaSettingsRange* value) { m_imageWidth = value; }

  String fillLightMode() const;
  void setFillLightMode(media::mojom::blink::FillLightMode fillLightMode) {
    m_fillLightMode = fillLightMode;
  }

  bool redEyeReduction() const { return m_redEyeReduction; }
  void setRedEyeReduction(bool redEyeReduction) {
    m_redEyeReduction = redEyeReduction;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  PhotoCapabilities() = default;

  Member<MediaSettingsRange> m_imageHeight;
  Member<MediaSettingsRange> m_imageWidth;
  media::mojom::blink::FillLightMode m_fillLightMode =
      media::mojom::blink::FillLightMode::NONE;
  bool m_redEyeReduction;
};

}  // namespace blink

#endif  // PhotoCapabilities_h
