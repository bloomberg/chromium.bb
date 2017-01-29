// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaValuesCached_h
#define MediaValuesCached_h

#include "core/CoreExport.h"
#include "core/css/MediaValues.h"
#include "platform/CrossThreadCopier.h"

namespace blink {

class CORE_EXPORT MediaValuesCached final : public MediaValues {
 public:
  struct CORE_EXPORT MediaValuesCachedData final {
    DISALLOW_NEW();
    // Members variables must be thread safe, since they're copied to the parser
    // thread
    double viewportWidth;
    double viewportHeight;
    int deviceWidth;
    int deviceHeight;
    float devicePixelRatio;
    int colorBitsPerComponent;
    int monochromeBitsPerComponent;
    PointerType primaryPointerType;
    int availablePointerTypes;
    HoverType primaryHoverType;
    int availableHoverTypes;
    int defaultFontSize;
    bool threeDEnabled;
    bool strictMode;
    String mediaType;
    WebDisplayMode displayMode;
    DisplayShape displayShape;
    ColorSpaceGamut colorGamut;

    MediaValuesCachedData();
    explicit MediaValuesCachedData(Document&);

    MediaValuesCachedData deepCopy() const {
      MediaValuesCachedData data;
      data.viewportWidth = viewportWidth;
      data.viewportHeight = viewportHeight;
      data.deviceWidth = deviceWidth;
      data.deviceHeight = deviceHeight;
      data.devicePixelRatio = devicePixelRatio;
      data.colorBitsPerComponent = colorBitsPerComponent;
      data.monochromeBitsPerComponent = monochromeBitsPerComponent;
      data.primaryPointerType = primaryPointerType;
      data.availablePointerTypes = availablePointerTypes;
      data.primaryHoverType = primaryHoverType;
      data.availableHoverTypes = availableHoverTypes;
      data.defaultFontSize = defaultFontSize;
      data.threeDEnabled = threeDEnabled;
      data.strictMode = strictMode;
      data.mediaType = mediaType.isolatedCopy();
      data.displayMode = displayMode;
      data.displayShape = displayShape;
      data.colorGamut = colorGamut;
      return data;
    }
  };

  static MediaValuesCached* create();
  static MediaValuesCached* create(const MediaValuesCachedData&);
  MediaValues* copy() const override;
  bool computeLength(double value,
                     CSSPrimitiveValue::UnitType,
                     int& result) const override;
  bool computeLength(double value,
                     CSSPrimitiveValue::UnitType,
                     double& result) const override;

  double viewportWidth() const override;
  double viewportHeight() const override;
  int deviceWidth() const override;
  int deviceHeight() const override;
  float devicePixelRatio() const override;
  int colorBitsPerComponent() const override;
  int monochromeBitsPerComponent() const override;
  PointerType primaryPointerType() const override;
  int availablePointerTypes() const override;
  HoverType primaryHoverType() const override;
  int availableHoverTypes() const override;
  bool threeDEnabled() const override;
  bool strictMode() const override;
  Document* document() const override;
  bool hasValues() const override;
  const String mediaType() const override;
  WebDisplayMode displayMode() const override;
  DisplayShape displayShape() const override;
  ColorSpaceGamut colorGamut() const override;

  void overrideViewportDimensions(double width, double height) override;

 protected:
  MediaValuesCached();
  MediaValuesCached(LocalFrame*);
  MediaValuesCached(const MediaValuesCachedData&);

  MediaValuesCachedData m_data;
};

template <>
struct CrossThreadCopier<MediaValuesCached::MediaValuesCachedData> {
  typedef MediaValuesCached::MediaValuesCachedData Type;
  static Type copy(const MediaValuesCached::MediaValuesCachedData& data) {
    return data.deepCopy();
  }
};

}  // namespace blink

#endif  // MediaValuesCached_h
