// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_unpositioned_float.h"

namespace blink {
namespace {

#ifndef NDEBUG
void AppendFragmentOffsetAndSize(const NGPhysicalFragment* fragment,
                                 StringBuilder* string_builder) {
  string_builder->Append("offset:");
  if (fragment->IsPlaced())
    string_builder->Append(fragment->Offset().ToString());
  else
    string_builder->Append("unplaced");
  string_builder->Append(" size:");
  string_builder->Append(fragment->Size().ToString());
}

void AppendFragmentToString(const NGPhysicalFragment* fragment,
                            StringBuilder* string_builder,
                            unsigned indent = 2) {
  for (unsigned i = 0; i < indent; i++)
    string_builder->Append(" ");

  if (fragment->IsBox()) {
    string_builder->Append("PhysicalBoxFragment ");
    AppendFragmentOffsetAndSize(fragment, string_builder);

    const auto* box = ToNGPhysicalBoxFragment(fragment);
    string_builder->Append(" overflow:");
    string_builder->Append(box->OverflowSize().ToString());
    string_builder->Append("\n");

    const auto& children = box->Children();
    for (unsigned i = 0; i < children.size(); i++)
      AppendFragmentToString(children[i].Get(), string_builder, indent + 2);
    return;
  }

  if (fragment->IsLineBox()) {
    string_builder->Append("NGPhysicalLineBoxFragment ");
    AppendFragmentOffsetAndSize(fragment, string_builder);

    const auto* line_box = ToNGPhysicalLineBoxFragment(fragment);
    string_builder->Append("\n");

    const auto& children = line_box->Children();
    for (unsigned i = 0; i < children.size(); i++)
      AppendFragmentToString(children[i].Get(), string_builder, indent + 2);
    return;
  }

  if (fragment->IsText()) {
    string_builder->Append("PhysicalTextFragment ");
    AppendFragmentOffsetAndSize(fragment, string_builder);

    const auto* text = ToNGPhysicalTextFragment(fragment);
    string_builder->Append(" start: ");
    string_builder->Append(String::Format("%u", text->StartOffset()));
    string_builder->Append(" end: ");
    string_builder->Append(String::Format("%u", text->EndOffset()));
    string_builder->Append("\n");
    return;
  }

  string_builder->Append("Unknown fragment type ");
  AppendFragmentOffsetAndSize(fragment, string_builder);
  string_builder->Append("\n");
}
#endif  // !NDEBUG

}  // namespace

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       const ComputedStyle& style,
                                       NGPhysicalSize size,
                                       NGFragmentType type,
                                       RefPtr<NGBreakToken> break_token)
    : layout_object_(layout_object),
      style_(&style),
      size_(size),
      break_token_(std::move(break_token)),
      type_(type),
      is_placed_(false) {}

void NGPhysicalFragment::Destroy() const {
  switch (Type()) {
    case kFragmentBox:
      delete static_cast<const NGPhysicalBoxFragment*>(this);
      break;
    case kFragmentText:
      delete static_cast<const NGPhysicalTextFragment*>(this);
      break;
    case kFragmentLineBox:
      delete static_cast<const NGPhysicalLineBoxFragment*>(this);
      break;
    default:
      NOTREACHED();
      break;
  }
}

const ComputedStyle& NGPhysicalFragment::Style() const {
  DCHECK(style_);
  return *style_;
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalFragment::BorderWidths() const {
  unsigned edges = BorderEdges();
  NGPhysicalBoxStrut box_strut(
      LayoutUnit((edges & NGBorderEdges::kTop) ? Style().BorderTopWidth()
                                               : .0f),
      LayoutUnit((edges & NGBorderEdges::kRight) ? Style().BorderRightWidth()
                                                 : .0f),
      LayoutUnit((edges & NGBorderEdges::kBottom) ? Style().BorderBottomWidth()
                                                  : .0f),
      LayoutUnit((edges & NGBorderEdges::kLeft) ? Style().BorderLeftWidth()
                                                : .0f));
  return box_strut.SnapToDevicePixels();
}

RefPtr<NGPhysicalFragment> NGPhysicalFragment::CloneWithoutOffset() const {
  switch (Type()) {
    case kFragmentBox:
      return static_cast<const NGPhysicalBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentText:
      return static_cast<const NGPhysicalTextFragment*>(this)
          ->CloneWithoutOffset();
      break;
    case kFragmentLineBox:
      return static_cast<const NGPhysicalLineBoxFragment*>(this)
          ->CloneWithoutOffset();
      break;
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

String NGPhysicalFragment::ToString() const {
  return String::Format("Type: '%d' Size: '%s' Offset: '%s' Placed: '%d'",
                        Type(), Size().ToString().Ascii().data(),
                        Offset().ToString().Ascii().data(), IsPlaced());
}

#ifndef NDEBUG
void NGPhysicalFragment::ShowFragmentTree() const {
  StringBuilder string_builder;
  string_builder.Append(".:: LayoutNG Physical Fragment Tree ::.\n");
  AppendFragmentToString(this, &string_builder);
  fprintf(stderr, "%s\n", string_builder.ToString().Utf8().data());
}
#endif  // !NDEBUG

}  // namespace blink
