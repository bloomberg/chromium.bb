// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/TracedLayoutObject.h"

#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutText.h"
#include "core/layout/LayoutView.h"
#include <inttypes.h>
#include <memory>

namespace blink {

namespace {

void DumpToTracedValue(const LayoutObject& object,
                       bool trace_geometry,
                       TracedValue* traced_value) {
  traced_value->SetString(
      "address",
      String::Format("%" PRIxPTR, reinterpret_cast<uintptr_t>(&object)));
  traced_value->SetString("name", object.GetName());
  if (Node* node = object.GetNode()) {
    traced_value->SetString("tag", node->nodeName());
    if (node->IsElementNode()) {
      Element& element = ToElement(*node);
      if (element.HasID())
        traced_value->SetString("htmlId", element.GetIdAttribute());
      if (element.HasClass()) {
        traced_value->BeginArray("classNames");
        for (size_t i = 0; i < element.ClassNames().size(); ++i)
          traced_value->PushString(element.ClassNames()[i]);
        traced_value->EndArray();
      }
    }
  }

  // FIXME: When the fixmes in LayoutTreeAsText::writeLayoutObject() are
  // fixed, deduplicate it with this.
  if (trace_geometry) {
    traced_value->SetDouble("absX", object.AbsoluteBoundingBoxRect().X());
    traced_value->SetDouble("absY", object.AbsoluteBoundingBoxRect().Y());
    LayoutRect rect = object.DebugRect();
    traced_value->SetDouble("relX", rect.X());
    traced_value->SetDouble("relY", rect.Y());
    traced_value->SetDouble("width", rect.Width());
    traced_value->SetDouble("height", rect.Height());
  } else {
    traced_value->SetDouble("absX", 0);
    traced_value->SetDouble("absY", 0);
    traced_value->SetDouble("relX", 0);
    traced_value->SetDouble("relY", 0);
    traced_value->SetDouble("width", 0);
    traced_value->SetDouble("height", 0);
  }

  if (object.IsOutOfFlowPositioned())
    traced_value->SetBoolean("positioned", object.IsOutOfFlowPositioned());
  if (object.SelfNeedsLayout())
    traced_value->SetBoolean("selfNeeds", object.SelfNeedsLayout());
  if (object.NeedsPositionedMovementLayout())
    traced_value->SetBoolean("positionedMovement",
                             object.NeedsPositionedMovementLayout());
  if (object.NormalChildNeedsLayout())
    traced_value->SetBoolean("childNeeds", object.NormalChildNeedsLayout());
  if (object.PosChildNeedsLayout())
    traced_value->SetBoolean("posChildNeeds", object.PosChildNeedsLayout());

  if (object.IsTableCell()) {
    // Table layout might be dirty if traceGeometry is false.
    // See https://crbug.com/664271 .
    if (trace_geometry) {
      const LayoutTableCell& c = ToLayoutTableCell(object);
      traced_value->SetDouble("row", c.RowIndex());
      traced_value->SetDouble("col", c.AbsoluteColumnIndex());
      if (c.ResolvedRowSpan() != 1)
        traced_value->SetDouble("rowSpan", c.ResolvedRowSpan());
      if (c.ColSpan() != 1)
        traced_value->SetDouble("colSpan", c.ColSpan());
    } else {
      // At least indicate that object is a table cell.
      traced_value->SetDouble("row", 0);
      traced_value->SetDouble("col", 0);
    }
  }

  if (object.IsAnonymous())
    traced_value->SetBoolean("anonymous", object.IsAnonymous());
  if (object.IsRelPositioned())
    traced_value->SetBoolean("relativePositioned", object.IsRelPositioned());
  if (object.IsStickyPositioned())
    traced_value->SetBoolean("stickyPositioned", object.IsStickyPositioned());
  if (object.IsFloating())
    traced_value->SetBoolean("float", object.IsFloating());

  if (object.SlowFirstChild()) {
    traced_value->BeginArray("children");
    for (LayoutObject* child = object.SlowFirstChild(); child;
         child = child->NextSibling()) {
      traced_value->BeginDictionary();
      DumpToTracedValue(*child, trace_geometry, traced_value);
      traced_value->EndDictionary();
    }
    traced_value->EndArray();
  }
}

}  // namespace

std::unique_ptr<TracedValue> TracedLayoutObject::Create(const LayoutView& view,
                                                        bool trace_geometry) {
  std::unique_ptr<TracedValue> traced_value = TracedValue::Create();
  DumpToTracedValue(view, trace_geometry, traced_value.get());
  return traced_value;
}

}  // namespace blink
