// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryAsJSON_h
#define GeometryAsJSON_h

#include "platform/PlatformExport.h"
#include "platform/json/JSONValues.h"

namespace blink {

class TransformationMatrix;

template <typename T>
std::unique_ptr<JSONArray> PointAsJSONArray(const T& point) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(point.X());
  array->PushDouble(point.Y());
  return array;
}

template <typename T>
std::unique_ptr<JSONArray> SizeAsJSONArray(const T& size) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  array->PushDouble(size.Width());
  array->PushDouble(size.Height());
  return array;
}

PLATFORM_EXPORT std::unique_ptr<JSONArray> TransformAsJSONArray(
    const TransformationMatrix&);

}  // namespace blink

#endif  // GeometryAsJSON_h
