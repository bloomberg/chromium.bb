// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunkProperties.h"

#include "platform/wtf/text/WTFString.h"

namespace blink {

String PaintChunkProperties::ToString() const {
  StringBuilder sb;
  if (property_tree_state.Transform()) {
    sb.Append("transform=(");
    sb.Append(property_tree_state.Transform()->ToString());
    sb.Append(")");
  }
  if (property_tree_state.Clip()) {
    if (sb.length())
      sb.Append(", ");
    sb.Append("clip=(");
    sb.Append(property_tree_state.Clip()->ToString());
    sb.Append(")");
  }
  if (property_tree_state.Effect()) {
    if (sb.length())
      sb.Append(", ");
    sb.Append("effect=(");
    sb.Append(property_tree_state.Effect()->ToString());
    sb.Append(")");
  }
  if (sb.length())
    sb.Append(", ");
  sb.Append("backface_hidden=");
  sb.Append(backface_hidden ? "1" : "0");
  return sb.ToString();
}

}  // namespace blink
