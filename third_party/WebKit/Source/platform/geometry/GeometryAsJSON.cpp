// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/GeometryAsJSON.h"

#include "platform/transforms/TransformationMatrix.h"

namespace blink {

static double RoundCloseToZero(double number) {
  return std::abs(number) < 1e-7 ? 0 : number;
}

std::unique_ptr<JSONArray> TransformAsJSONArray(const TransformationMatrix& t) {
  std::unique_ptr<JSONArray> array = JSONArray::Create();
  {
    std::unique_ptr<JSONArray> row = JSONArray::Create();
    row->PushDouble(RoundCloseToZero(t.M11()));
    row->PushDouble(RoundCloseToZero(t.M12()));
    row->PushDouble(RoundCloseToZero(t.M13()));
    row->PushDouble(RoundCloseToZero(t.M14()));
    array->PushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::Create();
    row->PushDouble(RoundCloseToZero(t.M21()));
    row->PushDouble(RoundCloseToZero(t.M22()));
    row->PushDouble(RoundCloseToZero(t.M23()));
    row->PushDouble(RoundCloseToZero(t.M24()));
    array->PushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::Create();
    row->PushDouble(RoundCloseToZero(t.M31()));
    row->PushDouble(RoundCloseToZero(t.M32()));
    row->PushDouble(RoundCloseToZero(t.M33()));
    row->PushDouble(RoundCloseToZero(t.M34()));
    array->PushArray(std::move(row));
  }
  {
    std::unique_ptr<JSONArray> row = JSONArray::Create();
    row->PushDouble(RoundCloseToZero(t.M41()));
    row->PushDouble(RoundCloseToZero(t.M42()));
    row->PushDouble(RoundCloseToZero(t.M43()));
    row->PushDouble(RoundCloseToZero(t.M44()));
    array->PushArray(std::move(row));
  }
  return array;
}

}  // namespace blink
