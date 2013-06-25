// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/renderer/compositor_bindings/web_filter_operations_impl.h"

#include "base/basictypes.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace webkit {

#if WEB_FILTER_OPERATIONS_IS_VIRTUAL
WebFilterOperationsImpl::WebFilterOperationsImpl() {}

WebFilterOperationsImpl::~WebFilterOperationsImpl() {}

const cc::FilterOperations& WebFilterOperationsImpl::AsFilterOperations()
    const {
  return filter_operations_;
}

void WebFilterOperationsImpl::appendGrayscaleFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateGrayscaleFilter(amount));
}

void WebFilterOperationsImpl::appendSepiaFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateSepiaFilter(amount));
}

void WebFilterOperationsImpl::appendSaturateFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateSaturateFilter(amount));
}

void WebFilterOperationsImpl::appendHueRotateFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateHueRotateFilter(amount));
}

void WebFilterOperationsImpl::appendInvertFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateInvertFilter(amount));
}

void WebFilterOperationsImpl::appendBrightnessFilter(float amount) {
  filter_operations_.Append(
      cc::FilterOperation::CreateBrightnessFilter(amount));
}

void WebFilterOperationsImpl::appendContrastFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateContrastFilter(amount));
}

void WebFilterOperationsImpl::appendOpacityFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateOpacityFilter(amount));
}

void WebFilterOperationsImpl::appendBlurFilter(float amount) {
  filter_operations_.Append(cc::FilterOperation::CreateBlurFilter(amount));
}

void WebFilterOperationsImpl::appendDropShadowFilter(WebKit::WebPoint offset,
                                                     float std_deviation,
                                                     WebKit::WebColor color) {
  filter_operations_.Append(cc::FilterOperation::CreateDropShadowFilter(
      offset, std_deviation, color));
}

void WebFilterOperationsImpl::appendColorMatrixFilter(SkScalar matrix[20]) {
  filter_operations_.Append(
      cc::FilterOperation::CreateColorMatrixFilter(matrix));
}

void WebFilterOperationsImpl::appendZoomFilter(float amount, int inset) {
  filter_operations_.Append(
      cc::FilterOperation::CreateZoomFilter(amount, inset));
}

void WebFilterOperationsImpl::appendSaturatingBrightnessFilter(float amount) {
  filter_operations_.Append(
      cc::FilterOperation::CreateSaturatingBrightnessFilter(amount));
}

void WebFilterOperationsImpl::clear() {
  filter_operations_.Clear();
}
#else

#define COMPILE_ASSERT_MATCHING_ENUMS(cc_name, webkit_name)                  \
  COMPILE_ASSERT(static_cast<int>(cc_name) == static_cast<int>(webkit_name), \
                 mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::GRAYSCALE,
    WebKit::WebFilterOperation::FilterTypeGrayscale);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::SEPIA,
    WebKit::WebFilterOperation::FilterTypeSepia);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::SATURATE,
    WebKit::WebFilterOperation::FilterTypeSaturate);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::HUE_ROTATE,
    WebKit::WebFilterOperation::FilterTypeHueRotate);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::INVERT,
    WebKit::WebFilterOperation::FilterTypeInvert);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::BRIGHTNESS,
    WebKit::WebFilterOperation::FilterTypeBrightness);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::CONTRAST,
    WebKit::WebFilterOperation::FilterTypeContrast);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::OPACITY,
    WebKit::WebFilterOperation::FilterTypeOpacity);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::BLUR,
    WebKit::WebFilterOperation::FilterTypeBlur);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::DROP_SHADOW,
    WebKit::WebFilterOperation::FilterTypeDropShadow);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::COLOR_MATRIX,
    WebKit::WebFilterOperation::FilterTypeColorMatrix);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::ZOOM,
    WebKit::WebFilterOperation::FilterTypeZoom);
COMPILE_ASSERT_MATCHING_ENUMS(
    cc::FilterOperation::SATURATING_BRIGHTNESS,
    WebKit::WebFilterOperation::FilterTypeSaturatingBrightness);

namespace {
cc::FilterOperation ConvertWebFilterOperationToFilterOperation(
    const WebKit::WebFilterOperation& web_filter_operation) {
  cc::FilterOperation::FilterType type =
      static_cast<cc::FilterOperation::FilterType>(web_filter_operation.type());

  cc::FilterOperation filter_operation =
      cc::FilterOperation::CreateEmptyFilter();
  filter_operation.set_type(type);

  switch (type) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
    case cc::FilterOperation::BLUR:
      filter_operation.set_amount(web_filter_operation.amount());
      break;
    case cc::FilterOperation::DROP_SHADOW:
      filter_operation.set_drop_shadow_offset(
          web_filter_operation.dropShadowOffset());
      filter_operation.set_amount(web_filter_operation.amount());
      filter_operation.set_drop_shadow_color(
          web_filter_operation.dropShadowColor());
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      filter_operation.set_matrix(web_filter_operation.matrix());
      break;
    case cc::FilterOperation::ZOOM:
      filter_operation.set_amount(web_filter_operation.amount());
      filter_operation.set_zoom_inset(web_filter_operation.zoomInset());
      break;
  }

  return filter_operation;
}
}  // namespace

cc::FilterOperations ConvertWebFilterOperationsToFilterOperations(
    const WebKit::WebFilterOperations& web_filter_operations) {
  cc::FilterOperations filter_operations;
  for (size_t i = 0; i < web_filter_operations.size(); ++i) {
      filter_operations.Append(ConvertWebFilterOperationToFilterOperation(
          web_filter_operations.at(i)));
  }
  return filter_operations;
}
#endif  // WEB_FILTER_OPERATIONS_IS_VIRTUAL

}  // namespace webkit
