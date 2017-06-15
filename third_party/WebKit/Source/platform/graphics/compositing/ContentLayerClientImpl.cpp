// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/ContentLayerClientImpl.h"

namespace blink {

template class RasterInvalidationTrackingMap<const cc::Layer>;
static RasterInvalidationTrackingMap<const cc::Layer>&
CcLayersRasterInvalidationTrackingMap() {
  DEFINE_STATIC_LOCAL(RasterInvalidationTrackingMap<const cc::Layer>, map, ());
  return map;
}

ContentLayerClientImpl::~ContentLayerClientImpl() {
  CcLayersRasterInvalidationTrackingMap().Remove(cc_picture_layer_.get());
}

template <typename T>
static std::unique_ptr<JSONArray> PointAsJSONArray(const T& point) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(point.X());
  array->PushDouble(point.Y());
  return array;
}

template <typename T>
static std::unique_ptr<JSONArray> SizeAsJSONArray(const T& size) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(size.Width());
  array->PushDouble(size.Height());
  return array;
}

void ContentLayerClientImpl::ResetTrackedRasterInvalidations() {
  RasterInvalidationTracking* tracking =
      CcLayersRasterInvalidationTrackingMap().Find(cc_picture_layer_.get());
  if (!tracking)
    return;

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
    tracking->invalidations.clear();
  else
    CcLayersRasterInvalidationTrackingMap().Remove(cc_picture_layer_.get());
}

RasterInvalidationTracking&
ContentLayerClientImpl::EnsureRasterInvalidationTracking() {
  return CcLayersRasterInvalidationTrackingMap().Add(cc_picture_layer_.get());
}

std::unique_ptr<JSONObject> ContentLayerClientImpl::LayerAsJSON(
    LayerTreeFlags flags) {
  std::unique_ptr<JSONObject> json = JSONObject::Create();
  json->SetString("name", debug_name_);

  FloatPoint position(cc_picture_layer_->offset_to_transform_parent().x(),
                      cc_picture_layer_->offset_to_transform_parent().y());
  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  IntSize bounds(cc_picture_layer_->bounds().width(),
                 cc_picture_layer_->bounds().height());
  if (!bounds.IsEmpty())
    json->SetArray("bounds", SizeAsJSONArray(bounds));

  json->SetBoolean("contentsOpaque", cc_picture_layer_->contents_opaque());
  json->SetBoolean("drawsContent", cc_picture_layer_->DrawsContent());

  if (flags & kLayerTreeIncludesDebugInfo) {
    std::unique_ptr<JSONArray> paint_chunk_contents_array = JSONArray::Create();
    for (const auto& debug_data : paint_chunk_debug_data_) {
      paint_chunk_contents_array->PushValue(debug_data->Clone());
    }
    json->SetArray("paintChunkContents", std::move(paint_chunk_contents_array));
  }

  CcLayersRasterInvalidationTrackingMap().AsJSON(cc_picture_layer_.get(),
                                                 json.get());
  return json;
}

}  // namespace blink
